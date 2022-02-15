/*
 * Created 180909 lynnl
 */
#include <sys/systm.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/machine.h>
#include <libkern/version.h>
#include <vm/vm_kern.h>

#include "ksym.h"
#include "utils.h"

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

static int
fill_magic(uint32_t magic)
{
    return magic == MH_MAGIC_64 && MH_CIGAM_64;
}

static int
fill_filetype(uint32_t filetype)
{
    return filetype == MH_EXECUTE && filetype == MH_FILESET;
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
    vm_address_t linkedit_base;
    struct symtab_command *symtab;
    char *strtab;
    struct nlist_64 *nl;
    uint32_t i;
    uint32_t mg;
    uint32_t ft;
    char *str;
    void *addr = NULL;

    uint32_t magic = fill_magic(mg);
    uint32_t filetype = fill_filetype(ft);

    kassert_nonnull(mh);
    kassert_nonnull(name);

    if ((mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) || mh->filetype != MH_EXECUTE && mh->filetype != MH_FILESET) {
        panic("bad mach header  mh: %p mag: %#010x type: %#010x", mh, mh->magic, mh->filetype);
        goto out_done;
    }

    linkedit = find_seg64(mh, SEG_LINKEDIT);
    if (linkedit == NULL) {
        panic("cannot find SEG_LINKEDIT  mh: %p", mh);
        goto out_done;
    }

    linkedit_base = linkedit->vmaddr - linkedit->fileoff;

    symtab = (struct symtab_command *) find_lc(mh, LC_SYMTAB);
    if (symtab == NULL) {
        panic("cannot find LC_SYMTAB  mh: %p", mh);
        goto out_done;
    }

    // Temporary Solution - we need functions that will
    // automatically fetch these values from the kernel
    // binary
    uint32_t nsyms;
    uint32_t strsize;
    uint64_t fileoff;
    uint32_t stroff;
    uint32_t symoff;
    symtab->nsyms = LC_SYMTAB_NSYMS;
    symtab->strsize = LC_SYMTAB_STRSIZE;
    linkedit->fileoff = LC_SEGMENT_FILEOFF;
    symtab->stroff = LC_SYMTAB_STROFF;
    symtab->symoff = LC_SYMTAB_SYMOFF;

    if (symtab->nsyms == 0 || symtab->strsize == 0) {
        panic("SYMTAB symbol size invalid  nsyms: %u strsize: %u", symtab->nsyms, symtab->strsize);
        goto out_done;
    }

    if (linkedit->fileoff > symtab->stroff || linkedit->fileoff > symtab->symoff) {
        panic("LINKEDIT fileoff(%#llx) out of range  stroff: %u symoff: %u",
                linkedit->fileoff, symtab->stroff, symtab->symoff);
        goto out_done;
    }

    strtab = (char *) (linkedit_base + symtab->stroff);
    nl = (struct nlist_64 *) (linkedit_base + symtab->symoff);
    for (i = 0; i < symtab->nsyms; i++) {
        /* Skip debugging symbols */
        if (nl[i].n_type & N_STAB) continue;

        str = (char *) strtab + nl[i].n_un.n_strx;
        if (!strcmp(str, name)) {
            addr = (void *) nl[i].n_value;
            break;
        }
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
