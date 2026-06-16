//
// klookup.c
//
// Kernel symbol lookup function
//
////////////////////////////////////////////////////////////////////////////////
// License: 0BSD                                                              //
//                                                                            //
// Copyright (C) 2021 by Syncretic                                            //
//                                                                            //
// Permission to use, copy, modify, and/or distribute this software for any   //
// purpose with or without fee is hereby granted.                             //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES   //
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF           //
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR    //
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES     //
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      //
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR //
// IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <mach-o/nlist.h>
#include <libklookup/klookup.h>

/*
 * Static kernel base addresses per architecture.
 *
 * These are the un-slid virtual addresses of the kernel Mach-O header
 * as laid out on disk. At runtime the kernel slide (KASLR) is added.
 *
 * x86_64: 0xffffff8000200000
 *   Standard kernel base for Intel Macs since 10.8.
 *
 * ARM64:  0xfffffff007004000
 *   Standard kernel base for Apple Silicon (M1/M2/M3/M4) and A-series.
 *   This matches the static link address in the kernelcache Mach-O.
 *   Note: on ARM64 the kernel is a kernelcache (MachO-in-MachO); the
 *   outer header is at this address, the inner XNU header follows.
 */
#if defined(__arm64__) || defined(__aarch64__)
#define KERNEL_BASE         0xfffffff007004000ULL
#else
#define KERNEL_BASE         0xffffff8000200000ULL
#endif

#define LB_SEG_PRELINK_TEXT "__PRELINK_TEXT"

static struct segment_command_64 *
FindSegment64(struct mach_header_64 *MachHeader, const char *SegmentName)
{
    struct load_command *LoadCommands;
    struct segment_command_64 *Segment = NULL;
    struct segment_command_64 *tmpSegment;

    LoadCommands = (struct load_command *)((uintptr_t)MachHeader + sizeof(struct mach_header_64));
    while ((uintptr_t)LoadCommands < (uintptr_t)MachHeader + (uintptr_t)MachHeader->sizeofcmds)
    {
        if (LoadCommands->cmd == LC_SEGMENT_64)
        {
            tmpSegment = (struct segment_command_64 *)LoadCommands;
            if (!strcmp(tmpSegment->segname, SegmentName))
            {
                Segment = tmpSegment;
                break;
            }
        }
        LoadCommands = (struct load_command *)((uintptr_t)LoadCommands + (uintptr_t)LoadCommands->cmdsize);
    }
    return Segment;
}

void *
SymbolLookup(const char *Symbol)
{
    static struct symtab_command  *SymbolTable = NULL;
    static void                   *StringTable = NULL;
    static struct nlist_64        *NameList    = NULL;

    vm_offset_t SlideAddress = 0;
    int64_t     Slide;
    struct mach_header_64       *MachHeader;
    struct segment_command_64   *LinkEdit;
    struct load_command         *LoadCommands;
    struct nlist_64             *tmpNameList;
    void        *Address;
    uint64_t    i;

    if (SymbolTable == NULL || StringTable == NULL || NameList == NULL)
    {
        /*
         * Calculate the kernel ASLR slide.
         *
         * vm_kernel_unslide_or_perm_external() returns the static
         * (un-slid) address of a kernel symbol. By comparing that
         * against the runtime address of printf we get the slide.
         */
        vm_kernel_unslide_or_perm_external((vm_offset_t)(void *)printf, &SlideAddress);

        /*
         * Slide = runtime_addr - static_addr
         *
         * Cast carefully to avoid sign-conversion warnings:
         * both sides are pointer-sized unsigned values; the
         * difference may be negative (represented as a large
         * unsigned number), so we work in int64_t.
         */
        Slide = (int64_t)(uintptr_t)(void *)printf - (int64_t)SlideAddress;

        /*
         * Apply the slide to the static kernel base to find
         * the runtime address of the kernel Mach-O header.
         */
        MachHeader = (struct mach_header_64 *)(uintptr_t)((int64_t)KERNEL_BASE + Slide);

        if (MachHeader->magic != MH_MAGIC_64)
        {
            printf("\n\nKLOOKUP: BAD MAGIC HEADER\n\n");
            panic("KLOOKUP: Bad Mach-O Magic Header\n");
            return NULL;
        }

        /*
         * On Big Sur and later the __LINKEDIT segment is copied into
         * the __PRELINK_TEXT region in memory. Use that as our base
         * if present.
         */
        if (version_major >= BIGSUR_XNU_MAJOR_VERSION)
        {
            if ((LinkEdit = FindSegment64(MachHeader, LB_SEG_PRELINK_TEXT)) != NULL)
            {
                MachHeader = (struct mach_header_64 *)(uintptr_t)(LinkEdit->vmaddr);
            }
        }

        if (!(LinkEdit = FindSegment64(MachHeader, SEG_LINKEDIT)))
        {
            printf("\n\nKLOOKUP: __LINKEDIT NOT FOUND\n\n");
            panic("KLOOKUP: __LINKEDIT not found\n");
            return NULL;
        }

        SymbolTable  = NULL;
        LoadCommands = (struct load_command *)((uintptr_t)MachHeader + sizeof(struct mach_header_64));
        while ((uintptr_t)LoadCommands < (uintptr_t)MachHeader + (uintptr_t)MachHeader->sizeofcmds)
        {
            if (LoadCommands->cmd == LC_SYMTAB)
            {
                SymbolTable = (struct symtab_command *)LoadCommands;
                break;
            }
            LoadCommands = (struct load_command *)((uintptr_t)LoadCommands + (uintptr_t)LoadCommands->cmdsize);
        }

        if (SymbolTable == NULL)
        {
            printf("\n\nKLOOKUP: LC_SYMTAB NOT FOUND\n\n");
            panic("KLOOKUP: LC_SYMTAB not found\n");
            return NULL;
        }

        StringTable = (void *)(uintptr_t)((int64_t)(LinkEdit->vmaddr - LinkEdit->fileoff) + (int64_t)SymbolTable->stroff);
        NameList    = (struct nlist_64 *)(uintptr_t)((int64_t)(LinkEdit->vmaddr - LinkEdit->fileoff) + (int64_t)SymbolTable->symoff);
    }

    for (i = 0, Address = NULL, tmpNameList = NameList; i < SymbolTable->nsyms; ++i, ++tmpNameList)
    {
        char *str = (char *)((uintptr_t)StringTable + tmpNameList->n_un.n_strx);

        if (!strcmp(str, Symbol))
        {
            Address = (void *)(uintptr_t)tmpNameList->n_value;
            break;
        }
    }

    if (Address == NULL)
    {
        printf("\n\nKLOOKUP: SYMBOL '%s' NOT FOUND\n\n", Symbol);
        panic("KLOOKUP: Symbol '%s' not found\n", Symbol);
    }

    return Address;
}