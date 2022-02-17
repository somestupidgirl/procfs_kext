//
//  resolver.c
//  test
//
//  Created by 五千年木 on 2018/12/24.
//  Copyright © 2018年 osxkernel. All rights reserved.
//

#include "resolver.h"

#include "test.h"

//
// Original KernelResolver code by snare:
// https://github.com/snare/KernelResolver
//
//
// Since 10.8 apple removed kernel symbols from loaded image
// So we will have to load kernel image from disk and parse that
// Modified to parse static kernel images
//

/* Borrowed from kernel source. It doesn't exist in Kernel.framework. */
struct nlist_64 {
    union {
        uint32_t  n_strx;   /* index into the string table */
    } n_un;
    uint8_t n_type;         /* type flag, see below */
    uint8_t n_sect;         /* section number or NO_SECT */
    uint16_t n_desc;        /* see <mach-o/stab.h> */
    uint64_t n_value;       /* value of this symbol (or stab offset) */
};

struct segment_command_64* find_segment_64(const struct mach_header_64* mh, const char* segname)
{
    if (!mh) {
        return NULL;
    }
    
    if (mh->magic != MH_MAGIC_64) {
        return NULL;
    }
    
    if (!segname) {
        return NULL;
    }
    
    struct load_command *lc;
    struct segment_command_64 *seg, *foundseg = NULL;
    
    /* First LC begins straight after the mach header */
    lc = (struct load_command *)((uint64_t)mh + sizeof(struct mach_header_64));
    while ((uint64_t)lc < (uint64_t)mh + (uint64_t)mh->sizeofcmds) {
        if (lc->cmd == LC_SEGMENT_64) {
            /* Check load command's segment name */
            seg = (struct segment_command_64 *)lc;
            if (strcmp(seg->segname, segname) == 0) {
                foundseg = seg;
                break;
            }
        }
        
        /* Next LC */
        lc = (struct load_command *)((uint64_t)lc + (uint64_t)lc->cmdsize);
    }
    
    /* Return the segment (NULL if we didn't find it) */
    return foundseg;
}

struct section_64* find_section_64(struct segment_command_64 *seg, const char *name)
{
    struct section_64 *sect, *foundsect = NULL;
    u_int i = 0;
    
    /* First section begins straight after the segment header */
    for (i = 0, sect = (struct section_64 *)((uint64_t)seg + (uint64_t)sizeof(struct segment_command_64));
         i < seg->nsects;
         i++, sect = (struct section_64 *)((uint64_t)sect + sizeof(struct section_64)))
    {
        /* Check section name */
        if (strcmp(sect->sectname, name) == 0) {
            foundsect = sect;
            break;
        }
    }
    
    /* Return the section (NULL if we didn't find it) */
    return foundsect;
}

struct load_command *
find_load_command(struct mach_header_64 *mh, uint32_t cmd)
{
    struct load_command *lc, *foundlc;
    
    /* First LC begins straight after the mach header */
    lc = (struct load_command *)((uint64_t)mh + sizeof(struct mach_header_64));
    while ((uint64_t)lc < (uint64_t)mh + (uint64_t)mh->sizeofcmds) {
        if (lc->cmd == cmd) {
            foundlc = (struct load_command *)lc;
            break;
        }
        
        /* Next LC */
        lc = (struct load_command *)((uint64_t)lc + (uint64_t)lc->cmdsize);
    }
    
    /* Return the load command (NULL if we didn't find it) */
    return foundlc;
}

void *find_symbol(struct mach_header_64 *mh, const char *name, uint64_t loaded_base)
{
    printf("moonAgirl[find_symbol] start.\n");
    /*
     * Check header
     */
    if (mh->magic != MH_MAGIC_64) {
        printf("moonAgirl[find_symbol] mh->magic err.\n");
        printf("magic number doesn't match - 0x%x\n", mh->magic);
        return NULL;
    }
    
    printf("moonAgirl[find_symbol] magic.\n");
    
    /*
     * Find __TEXT - we need it for fixed kernel base
     */
    struct segment_command_64 *seg_text = find_segment_64(mh, SEG_TEXT);
    if (!seg_text) {
        printf("couldn't find __TEXT\n");
        return NULL;
    }
    printf("moonAgirl[find_symbol] seg_text.\n");
    
    uint64_t fixed_base = seg_text->vmaddr;
    
    /*
     * Find the LINKEDIT and SYMTAB sections
     */
    struct segment_command_64 *seg_linkedit = find_segment_64(mh, SEG_LINKEDIT);
    if (!seg_linkedit) {
        printf("couldn't find __LINKEDIT\n");
        return NULL;
    }
    printf("moonAgirl[find_symbol] seg_linkedit.\n");
    
    struct symtab_command *lc_symtab = (struct symtab_command *)find_load_command(mh, LC_SYMTAB);
    if (!lc_symtab) {
        printf("couldn't find SYMTAB\n");
        return NULL;
    }
    printf("moonAgirl[find_symbol] lc_symtab.\n");
    
    /*
     * Enumerate symbols until we find the one we're after
     */
    uintptr_t base = (uintptr_t)mh;
    void* strtab = (void*)(base + lc_symtab->stroff);
    void* symtab = (void*)(base + lc_symtab->symoff);
    
    //printf("Symbol table offset 0x%x (%p)\n", lc_symtab->symoff, symtab);
    //printf("String table offset 0x%x (%p)\n", lc_symtab->stroff, strtab);
    
    struct nlist_64* nl = (struct nlist_64 *)(symtab);
    for (uint64_t i = 0; i < lc_symtab->nsyms; i++, nl = (struct nlist_64 *)((uint64_t)nl + sizeof(struct nlist_64)))
    {
        const char* str = (const char *)strtab + nl->n_un.n_strx;
        if (strcmp(str, name) == 0) {
            /* Return relocated address */
            return (void*) (nl->n_value - fixed_base + loaded_base);
        }
    }
    printf("moonAgirl[find_symbol] end.\n");
    /* Return the address (NULL if we didn't find it) */
    return NULL;
}

//void* resolve_kernel_symbol(const char* name, uintptr_t loaded_kernel_base)
//{
//    printf("moonAgirl[resolve_kernel_symbol] start.\n");
//    void* addr = NULL;
//    errno_t err = 0;
//    
//    if (!name) {
//        printf("moonAgirl:[resolve_kernel_symbol] name.\n");
//        return NULL;
//    }
//    printf("moonAgirl[resolve_kernel_symbol] name:%s.\n",name);
//    
//    const char* image_path = "/mach_kernel";
//    if (version_major >= 14) {
//        printf("moonAgirl[resolve_kernel_symbol] version_major >= 14.\n");
//        image_path = "/System/Library/Kernels/kernel"; // Since yosemite mach_kernel is moved
//    }
//    
//    vfs_context_t context = vfs_context_create(NULL);
//    if(!context) {
//        printf("vfs_context_create failed: %d\n", err);
//        return NULL;
//    }
//    printf("moonAgirl[resolve_kernel_symbol] context.\n");
//    
//    char* data = NULL;
//    uio_t uio = NULL;
//    vnode_t vnode = NULL;
//    err = vnode_lookup(image_path, 0, &vnode, context);//image_path -> vnode
//    if (err) {
//        printf("vnode_lookup(%s) failed: %d\n", image_path, err);
//        goto done;
//    }
//
//    
//    // Read whole kernel file into memory.
//    // It is not very efficient but it is the easiest way to adapt existing parsing code
//    // For production builds we need to use less memory
//    
//    struct vnode_attr attr;
//    err = vnode_getattr(vnode, &attr, context);//get vnode attr -> attr
//    printf("moonAgirl[resolve_kernel_symbol] vnode_attr.va_uid:%u.\n",(__uint32_t)attr.va_uid);
//    printf("moonAgirl[resolve_kernel_symbol] vnode_attr.va_gid:%u.\n",(__uint32_t)attr.va_gid);
//    printf("moonAgirl[resolve_kernel_symbol] vnode_attr.va_mode:%u.\n",(__uint16_t)attr.va_mode);
//    printf("moonAgirl[resolve_kernel_symbol] vnode_attr.va_flags:%u.\n",(uint32_t)attr.va_flags);
//    printf("moonAgirl[resolve_kernel_symbol] vnode_attr.va_supported:%lu.\n",(uint64_t)attr.va_supported);
//    printf("moonAgirl[resolve_kernel_symbol] vnode_attr.va_active:%lu.\n",(uint64_t)attr.va_active);
//    if (err) {
//        printf("can't get vnode attr: %d\n", err);
//        goto done;
//    }
//    
//    printf("moonAgirl[resolve_kernel_symbol] err.\n");
//    
//    uint32_t data_size = (uint32_t)attr.va_data_size;
//    printf("moonAgirl[resolve_kernel_symbol] data_size:%u.\n",data_size);
//    
//    data = OSMalloc(data_size, g_tag);
//    if (!data) {
//        printf("Could not allocate kernel buffer\n");
//        goto done;
//    }
//    printf("moonAgirl[resolve_kernel_symbol] data.\n");
//    
//    uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
//    if (!uio) {
//        printf("uio_create failed: %d\n", err);
//        goto done;
//    }
//    printf("moonAgirl[resolve_kernel_symbol] uio.\n");
//    
//    err = uio_addiov(uio, CAST_USER_ADDR_T(data), data_size);
//    if (err) {
//        printf("uio_addiov failed: %d\n", err);
//        goto done;
//    }
//    printf("moonAgirl[resolve_kernel_symbol] err.\n");
//    
//    err = VNOP_READ(vnode, uio, 0, context);//read data -> uio
//    if (err) {
//        printf("VNOP_READ failed: %d\n", err);
//        goto done;
//    }
//    printf("moonAgirl[resolve_kernel_symbol] err.\n");
//    
//    printf("moonAgirl[resolve_kernel_symbol] data:%p.\n",data);
//    
//    addr = find_symbol((struct mach_header_64*)data, name, loaded_kernel_base);
//    
//    printf("moonAgirl[resolve_kernel_symbol] addr:%p.\n",addr);
//    
//done:
//    printf("moonAgirl[resolve_kernel_symbol] done.\n");
//    uio_free(uio);
//    OSFree(data, data_size, g_tag);
//    vnode_put(vnode);
//    vfs_context_rele(context);
//    
//    return addr;
//}
