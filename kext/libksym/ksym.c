/*
 * Based on:
 *      https://github.com/Ch4nc3n/HookSysCall
 *      https://github.com/leiless/ksymresolver
 *      https://github.com/wrfsh/sysent_hook
 *      https://github.com/squiffy/Masochist
 *      https://github.com/phdphuc/mac-a-mal
 */
#include <stddef.h>
#include <stdint.h>

#include <IOKit/IOLib.h>

#include <libkern/OSMalloc.h>

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

#include "ksym.h"
#include "utils.h"

extern OSMallocTag g_tag;

struct descriptor_idt
{
    uint16_t    offset_low;
    uint16_t    seg_selector;
    uint8_t     reserved;
    uint8_t     flag;
    uint16_t    offset_middle;
    uint32_t    offset_high;
    uint32_t    eserved2;
};

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
find_symbol(struct mach_header_64 *mh, const char *name, uint64_t loaded_base)
{
    vm_address_t kern_base, fixed_base, linkedit_base;
    vm_offset_t vm_kern_slide;
    struct segment_command_64 *seg_linkedit, *seg_text;
    struct symtab_command *sc, *lc_symtab;
    struct nlist_64 *nl;
    const char *str;
    void *addr;
    uint32_t i;

    kassert_nonnull(mh);
    kassert_nonnull(name);

    vm_kern_slide = get_vm_kernel_slide();
    if (vm_kern_slide == 0) {
        panic("get_vm_kernel_slide() failed");
        return NULL;
    }

    kern_base = KERN_TEXT_BASE + vm_kern_slide;

    mh = (struct mach_header_64 *) kern_base;

    if ((mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) || (mh->filetype != MH_EXECUTE && mh->filetype != MH_FILESET))
    {
        panic("bad mach header  mh: %p mag: %#010x type: %#010x", mh, mh->magic, mh->filetype);
        return NULL;
    }
    
    /*
     * Find __TEXT - we need it for fixed kernel base
     */
    seg_text = find_seg64(mh, SEG_TEXT);
    if (seg_text == NULL) {
        panic("couldn't find __TEXT\n");
        return NULL;
    }

    fixed_base = seg_text->vmaddr;

    if (fixed_base != kern_base) {
        panic("kern_base and fixed_base do not return the same value");
        return NULL;
    }
    
    /*
     * Find the LINKEDIT and SYMTAB sections
     */
    seg_linkedit = find_seg64(mh, SEG_LINKEDIT);
    if (seg_linkedit == NULL) {
        panic("cannot find SEG_LINKEDIT  mh: %p", mh);
        return NULL;
    }

    lc_symtab = (struct symtab_command *) find_lc(mh, LC_SYMTAB);
    if (lc_symtab == NULL) {
        panic("cannot find LC_SYMTAB  mh: %p", mh);
        return NULL;
    }

    sc = lc_symtab;

    if (seg_linkedit->fileoff > sc->stroff || seg_linkedit->fileoff > sc->symoff) {
        panic("LINKEDIT fileoff(%#llx) out of range  stroff: %u symoff: %u", seg_linkedit->fileoff, sc->stroff, sc->symoff);
        return NULL;
    }

    if (sc->nsyms == 0 || sc->strsize == 0) {
        panic("SYMTAB symbol size invalid  nsyms: %u strsize: %u", sc->nsyms, sc->strsize);
        return NULL;
    }

    /* Calculate the address to the symbol table and the string table */
    
    uint64_t symtab = (seg_linkedit->vmaddr + (lc_symtab->symoff) - seg_linkedit->fileoff);
    uint64_t strtab = (seg_linkedit->vmaddr + (lc_symtab->stroff - seg_linkedit->fileoff));

    nl = (struct nlist_64 *)symtab;
    
    /* Iterate through the symbol table until we find what we're lookig for */
    for(i = 0; i < lc_symtab->nsyms; i++) {
        if(!strcmp(name, (char *)(nl->n_un.n_strx + strtab))) {
            /* Found it! Return a pointer to the symbol */
            return (void *)nl->n_value;
        }
        /* next nlist please */
        nl = (struct nlist_64 *)(symtab + (i * sizeof(struct nlist_64)));
    }

    /* Return the address (NULL if we didn't find it) */
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
        panic("vfs_context_create failed: %d\n", err);
        return NULL;
    }

    uio_t uio = NULL;
    vnode_t vnode = NULL;
    err = vnode_lookup(image_path, 0, &vnode, context);
    if (err) {
        panic("vnode_lookup(%s) failed: %d\n", image_path, err);
        goto done;
    }
    
    // Read whole kernel file into memory.
    // It is not very efficient but it is the easiest way to adapt existing parsing code
    // For production builds we need to use less memory
    
    struct vnode_attr attr;
    err = vnode_getattr(vnode, &attr, context);
    if (err) {
        panic("can't get vnode attr: %d\n", err);
        goto done;
    }
    
    uint32_t data_size = (uint32_t)attr.va_data_size;
    data = OSMalloc(data_size, g_tag);
    if (!data) {
        panic("Could not allocate kernel buffer\n");
        goto done;
    }
    
    uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
    if (!uio) {
        panic("uio_create failed: %d\n", err);
        goto done;
    }
    
    err = uio_addiov(uio, CAST_USER_ADDR_T(data), data_size);
    if (err) {
        panic("uio_addiov failed: %d\n", err);
        goto done;
    }
    
    err = VNOP_READ(vnode, uio, 0, context);
    if (err) {
        panic("VNOP_READ failed: %d\n", err);
        goto done;
    }

    vm_offset_t kernel_base = KERN_TEXT_BASE;
    
    addr = find_symbol(data, name, kernel_base);
    
    if (addr == NULL) {
        panic("Could not resolve kernel symbol: %s", name);
        goto done;
    }
    
done:
    uio_free(uio);
    OSFree(data, data_size, g_tag);
    vnode_put(vnode);
    vfs_context_rele(context);
    
    return addr;
}
