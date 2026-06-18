//
// klookup.c
//
// Kernel symbol lookup - ARM64/x86_64 compatible
//
// SPDX-License-Identifier: 0BSD
// Copyright (C) 2021 Syncretic, 2024 Sunneva Jonsdottir
//

#include <mach-o/nlist.h>
#include <libkern/version.h>
#include <libklookup/klookup.h>

/*
 * Find LC_FILESET_ENTRY "com.apple.kernel" inside a fileset kernelcache.
 */
static struct mach_header_64 *
FindFilesetKernel(struct mach_header_64 *fileset)
{
    struct load_command *lc = (struct load_command *)((uintptr_t)fileset + sizeof(*fileset));
    uintptr_t end = (uintptr_t)fileset + fileset->sizeofcmds;
    while ((uintptr_t)lc < end) {
        if (lc->cmdsize == 0) break;
        if (lc->cmd == 0x80000038) { /* LC_FILESET_ENTRY */
            uint64_t vmaddr  = *(uint64_t *)((uintptr_t)lc + 8);
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
    /*
     * Use vm_kernel_unslide_or_perm_external on the exported `version`
     * string to get the KASLR slide. Then walk backwards from `version`
     * page-by-page to find the Mach-O header.
     *
     * `version` is in __TEXT which is contiguous, so this walk is safe
     * within the same segment — we stop as soon as we find the header
     * or exceed a reasonable distance (16MB).
     */
    vm_offset_t unslid_version = 0;
    vm_kernel_unslide_or_perm_external((vm_offset_t)(void *)version, &unslid_version);
    if (unslid_version == 0) {
        printf("KLOOKUP: failed to get unslid version address\n");
        return NULL;
    }
    int64_t slide = (int64_t)(uintptr_t)(void *)version - (int64_t)unslid_version;
    printf("KLOOKUP: KASLR slide=0x%llx\n", (unsigned long long)slide);

    /*
     * Walk backwards from `version` looking for MH_MAGIC_64.
     * Limit to 64MB to cover the largest kernelcaches.
     */
    uintptr_t addr = (uintptr_t)(void *)version & ~((uintptr_t)PAGE_SIZE - 1);
    uintptr_t limit = addr > (64ULL * 1024 * 1024) ? addr - (64ULL * 1024 * 1024) : 0;

    while (addr > limit) {
        struct mach_header_64 *hdr = (struct mach_header_64 *)addr;
        if (hdr->magic == MH_MAGIC_64) {
            printf("KLOOKUP: found Mach-O header at %p filetype=%u\n",
                   hdr, hdr->filetype);
            if (hdr->filetype == MH_EXECUTE) {
                return hdr;
            }
            if (hdr->filetype == MH_FILESET) {
                struct mach_header_64 *kernel = FindFilesetKernel(hdr);
                if (kernel != NULL) {
                    printf("KLOOKUP: com.apple.kernel slice at %p\n", kernel);
                    return kernel;
                }
                return hdr;
            }
        }
        addr -= PAGE_SIZE;
    }

    printf("KLOOKUP: Mach-O header not found within 64MB of version string\n");
    return NULL;
}

static struct segment_command_64 *
FindSegment64(struct mach_header_64 *mh, const char *name)
{
    struct load_command *lc = (struct load_command *)((uintptr_t)mh + sizeof(*mh));
    while ((uintptr_t)lc < (uintptr_t)mh + mh->sizeofcmds) {
        if (lc->cmdsize == 0) break;
        if (lc->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *seg = (struct segment_command_64 *)lc;
            if (!strncmp(seg->segname, name, 16))
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

        struct segment_command_64 *prelink = FindSegment64(mh, "__PRELINK_TEXT");
        if (prelink != NULL) {
            struct mach_header_64 *inner = (struct mach_header_64 *)(uintptr_t)prelink->vmaddr;
            if (inner->magic == MH_MAGIC_64) {
                printf("KLOOKUP: using __PRELINK_TEXT inner header at %p\n", inner);
                mh = inner;
            }
        }

        struct segment_command_64 *linkedit = FindSegment64(mh, SEG_LINKEDIT);
        if (linkedit == NULL) {
            printf("KLOOKUP: __LINKEDIT not found\n");
            return NULL;
        }

        struct load_command *lc = (struct load_command *)((uintptr_t)mh + sizeof(*mh));
        while ((uintptr_t)lc < (uintptr_t)mh + mh->sizeofcmds) {
            if (lc->cmdsize == 0) break;
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

        printf("KLOOKUP: %u symbols at %p\n", SymbolTable->nsyms, NameList);
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
