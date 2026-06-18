//
// klookup.c
//
// Kernel symbol lookup - ARM64/x86_64 compatible
//
// SPDX-License-Identifier: 0BSD
// Copyright (C) 2021 Syncretic, 2024 Sunneva Jonsdottir
//

#include <mach-o/nlist.h>
#include <libklookup/klookup.h>

/*
 * Find a LC_FILESET_ENTRY named "com.apple.kernel" inside a fileset kernelcache.
 * Returns the embedded XNU Mach-O header, or NULL if not found.
 */
static struct mach_header_64 *
FindFilesetKernel(struct mach_header_64 *fileset)
{
    struct load_command *lc = (struct load_command *)((uintptr_t)fileset + sizeof(*fileset));
    while ((uintptr_t)lc < (uintptr_t)fileset + fileset->sizeofcmds) {
        /* LC_FILESET_ENTRY = 0x80000038 */
        if (lc->cmd == 0x80000038) {
            /* fileset entry: uint64_t vmaddr, uint64_t fileoff, uint32_t entry_id_offset */
            uint64_t *fields = (uint64_t *)((uintptr_t)lc + sizeof(struct load_command));
            uint64_t vmaddr = fields[0];
            uint32_t nameoff = *(uint32_t *)((uintptr_t)lc + 24);
            const char *name = (const char *)((uintptr_t)lc + nameoff);
            if (!strcmp(name, "com.apple.kernel")) {
                return (struct mach_header_64 *)(uintptr_t)vmaddr;
            }
        }
        lc = (struct load_command *)((uintptr_t)lc + lc->cmdsize);
    }
    return NULL;
}

static struct mach_header_64 *
FindKernelBase(void)
{
    vm_offset_t unslid = 0;
    vm_kernel_unslide_or_perm_external((vm_offset_t)(void *)printf, &unslid);
    int64_t slide = (int64_t)(uintptr_t)(void *)printf - (int64_t)unslid;

    /*
     * Walk backwards from printf in PAGE_SIZE steps looking for
     * the Mach-O magic. Limit search to 512MB to avoid infinite loop.
     */
    uintptr_t addr = (uintptr_t)(void *)printf & ~((uintptr_t)PAGE_SIZE - 1);
    uintptr_t limit = addr - (512ULL * 1024 * 1024);

    while (addr > limit) {
        struct mach_header_64 *hdr = (struct mach_header_64 *)addr;
        if (hdr->magic == MH_MAGIC_64) {
            if (hdr->filetype == MH_EXECUTE) {
                return hdr;
            }
            if (hdr->filetype == MH_FILESET) {
                /* macOS 12+ fileset kernelcache - find the XNU kernel slice */
                struct mach_header_64 *kernel = FindFilesetKernel(hdr);
                if (kernel != NULL)
                    return kernel;
                return hdr;
            }
        }
        addr -= PAGE_SIZE;
    }

    (void)slide;
    return NULL;
}

static struct segment_command_64 *
FindSegment64(struct mach_header_64 *mh, const char *name)
{
    struct load_command *lc = (struct load_command *)((uintptr_t)mh + sizeof(*mh));
    while ((uintptr_t)lc < (uintptr_t)mh + mh->sizeofcmds) {
        if (lc->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *seg = (struct segment_command_64 *)lc;
            if (!strcmp(seg->segname, name))
                return seg;
        }
        lc = (struct load_command *)((uintptr_t)lc + lc->cmdsize);
    }
    return NULL;
}

void *
SymbolLookup(const char *Symbol)
{
    static struct symtab_command *SymbolTable = NULL;
    static void                  *StringTable = NULL;
    static struct nlist_64       *NameList    = NULL;

    if (SymbolTable == NULL || StringTable == NULL || NameList == NULL) {
        struct mach_header_64 *mh = FindKernelBase();
        if (mh == NULL) {
            printf("KLOOKUP: failed to find kernel base\n");
            return NULL;
        }

        /* On macOS 11+ __LINKEDIT is in __PRELINK_TEXT region */
        struct segment_command_64 *prelink = FindSegment64(mh, "__PRELINK_TEXT");
        if (prelink != NULL) {
            struct mach_header_64 *inner = (struct mach_header_64 *)(uintptr_t)prelink->vmaddr;
            if (inner->magic == MH_MAGIC_64)
                mh = inner;
        }

        struct segment_command_64 *linkedit = FindSegment64(mh, SEG_LINKEDIT);
        if (linkedit == NULL) {
            printf("KLOOKUP: __LINKEDIT not found\n");
            return NULL;
        }

        struct load_command *lc = (struct load_command *)((uintptr_t)mh + sizeof(*mh));
        while ((uintptr_t)lc < (uintptr_t)mh + mh->sizeofcmds) {
            if (lc->cmd == LC_SYMTAB) {
                SymbolTable = (struct symtab_command *)lc;
                break;
            }
            lc = (struct load_command *)((uintptr_t)lc + lc->cmdsize);
        }

        if (SymbolTable == NULL) {
            printf("KLOOKUP: LC_SYMTAB not found\n");
            return NULL;
        }

        int64_t base = (int64_t)linkedit->vmaddr - (int64_t)linkedit->fileoff;
        StringTable = (void *)(uintptr_t)(base + (int64_t)SymbolTable->stroff);
        NameList    = (struct nlist_64 *)(uintptr_t)(base + (int64_t)SymbolTable->symoff);
    }

    struct nlist_64 *nl = NameList;
    for (uint64_t i = 0; i < SymbolTable->nsyms; i++, nl++) {
        char *str = (char *)((uintptr_t)StringTable + nl->n_un.n_strx);
        if (!strcmp(str, Symbol))
            return (void *)(uintptr_t)nl->n_value;
    }

    printf("KLOOKUP: symbol '%s' not found\n", Symbol);
    return NULL;
}
