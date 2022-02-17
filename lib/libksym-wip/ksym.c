/*
 * Based on:
 *      https://github.com/Ch4nc3n/HookSysCall
 *      https://github.com/leiless/ksymresolver
 *      https://github.com/wrfsh/sysent_hook
 *      https://github.com/squiffy/Masochist
 *      https://github.com/phdphuc/mac-a-mal
 *      https://github.com/reenigneorcim/latebloom
 */

#include <stddef.h>
#include <stdint.h>

#include <IOKit/IOLib.h>

#include <libkern/OSMalloc.h>
#include <libkern/version.h>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include <mach/machine.h>
#include <mach/vm_types.h>

#include <sys/cdefs.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>

#include <vm/vm_kern.h>

#include <libkext/libkext.h>

#include "ksym.h"

extern OSMallocTag g_tag;

#pragma mark -
#pragma mark Static Functions

/**
 * Get value of global variable vm_kernel_addrperm_ext(since 10.11)
 * @return      0 if failed to get
 * see: xnu/osfmk/vm/vm_kern.c#vm_kernel_addrperm_external
 */
static vm_offset_t
get_vm_kernel_addrperm_ext(void)
{
    static vm_offset_t addrperm_ext = 0L;
    if (addrperm_ext != 0L) goto out_exit;
    vm_kernel_addrperm_external(VM_MIN_KERNEL_AND_KEXT_ADDRESS, &addrperm_ext);
    addrperm_ext -= VM_MIN_KERNEL_AND_KEXT_ADDRESS;

out_exit:
    return addrperm_ext;
}

/**
 * Get value of global variable vm_kernel_slide(since 10.11)
 * @return      0 if failed to get
 * see: xnu/osfmk/vm/vm_kern.c#vm_kernel_unslide_or_perm_external
 */
static vm_offset_t
get_vm_kernel_slide(void)
{
    static uint16_t i = MAX_SLIDE_STEP;
    static vm_offset_t fake = VM_MIN_KERNEL_AND_KEXT_ADDRESS;
    static vm_offset_t slide = 0L;

    if (get_vm_kernel_addrperm_ext() == 0L) goto out_exit;
    if (slide != 0L || i == 0) goto out_exit;

    while (--i) {
        vm_kernel_unslide_or_perm_external(fake, &slide);
        /* We assume unslide address is unique to those two */
        if (slide != fake && slide != fake + get_vm_kernel_addrperm_ext()) {
            slide = fake - slide;
            break;
        }
        fake += 0x100000L;
        slide = 0L;
    }

out_exit:
    return slide;
}

static struct segment_command_64 *
find_seg64(struct mach_header_64 *mh, const char *name)
{
    uint32_t i;
    struct load_command *lc;
    struct segment_command_64 *seg;

    lc = (struct load_command *) ((uint8_t *) mh + sizeof(*mh));
    for (i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_64) {
            seg = (struct segment_command_64 *) lc;
            if (!strcmp(seg->segname, name)) return seg;
        }

        lc = (struct load_command *) ((uint8_t *) lc + lc->cmdsize);
    }

    return NULL;
}

static struct load_command *
find_lc(struct mach_header_64 *mh, uint32_t cmd)
{
    uint32_t i;
    struct load_command *lc;

    /* First LC begins straight after the mach header */
    lc = (struct load_command *) ((uint8_t *) mh + sizeof(*mh));
    for (i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == cmd) return lc;
        lc = (struct load_command *) ((uint8_t *) lc + lc->cmdsize);
    }

    return NULL;
}

static void *
find_symbol(struct mach_header_64 *mh, const char *name)
{
    vm_address_t kern_base;
    vm_offset_t vm_kern_slide;
    struct segment_command_64 *linkedit;
    struct symtab_command *symtab;
    struct nlist_64 *nl, *nlist;
    void *addr, *strtab;
    const char *str;
    uint32_t i;

    kassert_nonnull(mh);
    kassert_nonnull(name);

    vm_kern_slide = get_vm_kernel_slide();
    kern_base = KERN_TEXT_BASE + vm_kern_slide;
    mh = (struct mach_header_64 *) kern_base;

    if ((mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) || (mh->filetype != MH_EXECUTE && mh->filetype != MH_FILESET)) {
        IOLog("Bad mach header | mh: %p | mag: %#010x | type: %#010x", mh, mh->magic, mh->filetype);
        return NULL;
    }

    if (version_major >= BIGSUR_XNU_MAJOR_VERSION) {
        /* Find __PRELINK_TEXT for Big Sur and up  */
        linkedit = find_seg64(mh, SEG_PRELINK_TEXT);
        if (linkedit == NULL) {
            IOLog("Cannot find __PRELINK_TEXT\n");
            return NULL;
        }
    } else {
        /* Find __LINKEDIT */
        linkedit = find_seg64(mh, SEG_LINKEDIT);
        if (linkedit == NULL) {
            IOLog("Cannot find __LINKEDIT | mh: %p", mh);
            return NULL;
        }
    }

    /* Find LC_SYMTAB */
    symtab = (struct symtab_command *) find_lc(mh, LC_SYMTAB);
    if (symtab == NULL) {
        IOLog("Cannot find LC_SYMTAB | mh: %p", mh);
        return NULL;
    }

    // Get the address of the string table
    strtab = (void *)((int64_t)(linkedit->vmaddr - linkedit->fileoff) + symtab->stroff);
    nlist = (struct nlist_64 *)((int64_t)(linkedit->vmaddr - linkedit->fileoff) + symtab->symoff);

    for (i = 0, name = NULL, nl = nlist; i < symtab->nsyms; ++i, ++nl) {
        char *str = (char *)(strtab + nl->n_un.n_strx);
        if (!strcmp(str, name)) {
            return (void *)nl->n_value;
            break;
        }
    }

#if 0
    if (symtab->nsyms == 0 || symtab->strsize == 0) {
        IOLog("SYMTAB symbol size invalid  nsyms: %u strsize: %u", symtab->nsyms, symtab->strsize);
        return NULL;
    }

    if (linkedit->fileoff > symtab->stroff || linkedit->fileoff > symtab->symoff) {
        IOLog("LINKEDIT fileoff(%#llx) out of range  stroff: %u symoff: %u", linkedit->fileoff, symtab->stroff, symtab->symoff);
        return NULL;
    }
#endif

    /* Return NULL if we didn't find the symbol address */
    return NULL;
}

#pragma mark -
#pragma mark Global Functions

void*
resolve_kernel_symbol(const char* name)
{
    struct mach_header_64 *mh, *data;
    void* addr = NULL;
    errno_t err = 0;
    
    if (!name) {
        return NULL;
    }
    
    const char* image_path = "/System/Library/Kernels/kernel";
    
    vfs_context_t context = vfs_context_create(NULL);
    if(!context) {
        IOLog("vfs_context_create failed: %d\n", err);
        return NULL;
    }

    uio_t uio = NULL;
    vnode_t vnode = NULL;
    err = vnode_lookup(image_path, 0, &vnode, context);
    if (err) {
        IOLog("vnode_lookup(%s) failed: %d\n", image_path, err);
        goto done;
    }
    
    // Read whole kernel file into memory.
    // It is not very efficient but it is the easiest way to adapt existing parsing code
    // For production builds we need to use less memory
    
    struct vnode_attr attr;
    err = vnode_getattr(vnode, &attr, context);
    if (err) {
        IOLog("can't get vnode attr: %d\n", err);
        goto done;
    }
    
    uint32_t data_size = (uint32_t)attr.va_data_size;
    data = OSMalloc(data_size, g_tag);
    if (!data) {
        IOLog("Could not allocate kernel buffer\n");
        goto done;
    }
    
    uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
    if (!uio) {
        IOLog("uio_create failed: %d\n", err);
        goto done;
    }
    
    err = uio_addiov(uio, CAST_USER_ADDR_T(data), data_size);
    if (err) {
        IOLog("uio_addiov failed: %d\n", err);
        goto done;
    }
    
    err = VNOP_READ(vnode, uio, 0, context);
    if (err) {
        IOLog("VNOP_READ failed: %d\n", err);
        goto done;
    }
    
    addr = find_symbol(data, name);
    
    if (addr == NULL) {
        IOLog("Could not resolve kernel symbol: %s", name);
        goto done;
    }
    
done:
    uio_free(uio);
    OSFree(data, data_size, g_tag);
    vnode_put(vnode);
    vfs_context_rele(context);
    
    return addr;
}
