/*
 * Created 180909 lynnl
 */
#include <stddef.h>
#include <sys/systm.h>
#include <kern/clock.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/machine.h>
#include <os/log.h>
#include <libkern/version.h>
#include <vm/vm_kern.h>

#include "ksym.h"
#include "utils.h"

#pragma mark -
#pragma mark Static Arrays

static char *kernel_paths[] = {
    "/mach_kernel",
    "/System/Library/Kernels/kernel",
    "/System/Library/Kernels/kernel.development",
    "/System/Library/Kernels/kernel.debug"
    "/System/Library/Kernels/kernel.release.t8020"
    "/System/Library/Kernels/Kernel.release.t8101"
};

#pragma mark -
#pragma mark Functions

/*
 * Find __TEXT - we need it for fixed kernel base
 *
 * Note: Returns the same output as KERN_TEXT_BASE + vm_kern_slide:
 *    vm_address_t kern_base;
 *    vm_kern_slide = get_vm_kernel_slide();
 *    kern_base = KERN_TEXT_BASE + vm_kern_slide;
 *    text_base = get_text_base;
 *    printf("text base: %#018lx kern_base: %#018lx", text_base, kern_base);
 *
 *    text_base: 0xffffff800dc00000 kern_base: 0xffffff800dc00000
 */
vm_address_t
get_text_base(struct mach_header_64 *mh)
{
    struct segment_command_64 *text;
    vm_address_t text_base;

    text = find_seg64(mh, SEG_TEXT);
    if (!text) {
        panic("cannot find SEG_TEXT  mh: %p", mh);
    }

    text_base = text->vmaddr;

    return text_base;
}

/*
 *  vm_address_t linkedit_base;
 *  linkedit_base = get_linkedit_base(mh);
 *  printf("linkedit_base: %#018lx", linkedit_base);
 *
 * Returns: 
 *  linkedit_base: 0xffffff800f160000
 */
vm_address_t
get_linkedit_base(struct mach_header_64 *mh)
{
    struct segment_command_64 *linkedit;
    vm_address_t linkedit_base;

    linkedit = find_seg64(mh, SEG_LINKEDIT);
    if (!linkedit) {
        panic("cannot find SEG_LINKEDIT  mh: %p", mh);
    }

    linkedit_base = linkedit->vmaddr - linkedit->fileoff;

    return linkedit_base;
}

/**
 * Get value of global variable vm_kernel_addrperm_ext(since 10.11)
 * @return      0 if failed to get
 * see: xnu/osfmk/vm/vm_kern.c#vm_kernel_addrperm_external
 */
vm_offset_t
get_vm_kernel_addrperm_ext(void)
{
    static vm_offset_t addrperm_ext = 0L;
    if (addrperm_ext != 0L) {
        goto out_exit;
    }
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
vm_offset_t
get_vm_kernel_slide(void)
{
    uint16_t i = MAX_SLIDE_STEP;
    vm_offset_t fake = VM_MIN_KERNEL_AND_KEXT_ADDRESS;
    vm_offset_t slide = 0L;

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

struct segment_command_64 *
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

struct load_command *
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

/**
 * Find a symbol from symtab
 * keywords: kernel_nlist_t  struct nlist_64
 * see: xnu/libkern/c++/OSKext.cpp#slidePrelinkedExecutable
 */
static void *
resolve_ksymbol2(struct mach_header_64 *mh, const char *name)
{
    struct segment_command_64 *linkedit;
    struct symtab_command *symtab;
    struct nlist_64 *nl;
    vm_address_t kern_base;
    vm_address_t linkedit_base;
    vm_address_t fixed_base;
    char *str, *strtab;
    void *addr = NULL;
    uint32_t i;

    fixed_base = get_text_base(mh);
    linkedit_base = get_linkedit_base(mh);

    /*
     * Check header
     */
    mh = (struct mach_header_64 *) fixed_base;
    if ((mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) || (mh->filetype != MH_EXECUTE && mh->filetype != MH_FILESET))
    {
        panic("bad mach header  mh: %p mag: %#010x type: %#010x", mh, mh->magic, mh->filetype);
        goto out_done;
    }

    /*
     * Find the SYMTAB section
     */
    symtab = (struct symtab_command *) find_lc(mh, LC_SYMTAB);
    if (!symtab) {
        panic("cannot find LC_SYMTAB  mh: %p", mh);
        goto out_done;
    }

    uint32_t symbol_offset = offsetof(struct symtab_command, symoff);
    uint32_t num_symbols;
    uint32_t string_offset = offsetof(struct symtab_command, stroff);
    uint32_t string_size = sizeof(name);

    symtab->symoff = symbol_offset;
    symtab->nsyms = num_symbols;
    symtab->stroff = string_offset;
    symtab->strsize = string_size;

    strtab = (char *) (linkedit_base + symtab->stroff);
    symtab = (char *) (linkedit_base + symtab->symoff);
    nl = (struct nlist_64 *) (linkedit_base + symtab->symoff);

    /*
     * Enumerate symbols until we find the one we're after
     */
    for (i = 0; i < symtab->nsyms; i++) {
        /* Skip debugging symbols */
        if (nl[i].n_type & N_STAB) continue;

        str = (char *) strtab + nl[i].n_un.n_strx;
        if (!strcmp(str, name)) {
            addr = (void *) nl[i].n_value;
            break;
        }
    }

    if (symtab->nsyms == 0 || symtab->strsize == 0) {
        panic("SYMTAB symbol size invalid  nsyms: %u strsize: %u", symtab->nsyms, symtab->strsize);
        goto out_done;
    }

    if (linkedit->fileoff > symtab->stroff || linkedit->fileoff > symtab->symoff) {
        panic("LINKEDIT fileoff(%#llx) out of range  stroff: %u symoff: %u",
                linkedit->fileoff, symtab->stroff, symtab->symoff);
        goto out_done;
    }

out_done:
    return addr;
}

/**
 * Resolve a kernel symbol address
 * @param name          symbol name(should begin with _)
 * @return              NULL if not found
 */
void * __nullable
resolve_ksymbol(const char * __nonnull name)
{
    static volatile struct mach_header_64 *mh = NULL;

    if (mh == NULL) {
        mh = (struct mach_header_64 *) (KERN_TEXT_BASE + get_vm_kernel_slide());
    }

    return resolve_ksymbol2((struct mach_header_64 *) mh, name);
}
