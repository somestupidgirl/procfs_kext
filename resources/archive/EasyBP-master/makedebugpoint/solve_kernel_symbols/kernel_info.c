//
//  kernel_info.c
//  makedebugpoint
//
//  Created by zuff on 2019/3/1.
//  Copyright © 2019 zuff. All rights reserved.
//

#include "kernel_info.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/vnode.h>
#include <string.h>
#include <sys/attr.h>
#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <vm/vm_kern.h>

#include "configuration.h"

static kern_return_t get_kernel_mach_header(void *buffer, vnode_t kernel_vnode, vfs_context_t vfs);
static kern_return_t process_kernel_mach_header(void *kernel_header, struct kernel_info *kinfo);
static kern_return_t get_kernel_linkedit(vnode_t kernel_vnode, vfs_context_t vfs, struct kernel_info *kinfo);
static void get_running_kernel_address(struct kernel_info *kinfo);
static mach_vm_address_t find_kernel_kaslr(void);

mach_header_t *g_kernel_header = NULL;


/*
 * entrypoint function to read necessary information from running kernel and kernel at disk
 * such as kaslr slide, linkedit location
 * the reads from disk are implemented using the available KPI VFS functions
 */
kern_return_t
init_kernel_info(struct kernel_info *kinfo)
{
    //moony_modify//printf("[DEBUG] init_kernel_info entered:\n");
    kern_return_t error = 0;
    // lookup vnode for /mach_kernel
    vnode_t kernel_vnode = NULLVP;
    vfs_context_t vfs = NULL;
    if ((vfs = vfs_context_create(NULL)) == NULL)
    {
        return KERN_FAILURE;
    }
    error = vnode_lookup(MACH_KERNEL, 0, &kernel_vnode, vfs);
    if (error)
    {
        return KERN_FAILURE;
    }
    
    void *kernel_header = _MALLOC(PAGE_SIZE_64, M_TEMP, M_ZERO);
    if (kernel_header == NULL) return KERN_FAILURE;
    
    error = get_kernel_mach_header(kernel_header, kernel_vnode, vfs);
    if (error) goto failure;
    error = process_kernel_mach_header(kernel_header, kinfo);
    if (error) goto failure;
    
    kinfo->kaslr_slide = find_kernel_kaslr();
    get_running_kernel_address(kinfo);

    kinfo->linkedit_buf = _MALLOC(kinfo->linkedit_size, M_TEMP, M_ZERO);
    if (kinfo->linkedit_buf == NULL)
    {
        _FREE(kernel_header, M_TEMP);
        return KERN_FAILURE;
    }
    
    error = get_kernel_linkedit(kernel_vnode,vfs, kinfo);
    if (error) goto failure;
    
success:
    if (vfs)
    {
        vfs_context_rele(vfs);
    }
    _FREE(kernel_header, M_TEMP);
    
    vnode_put(kernel_vnode);
    return KERN_SUCCESS;
    
failure:
    if (vfs)
    {
        vfs_context_rele(vfs);
    }
    if (kinfo->linkedit_buf != NULL) _FREE(kinfo->linkedit_buf, M_TEMP);
    _FREE(kernel_header, M_TEMP);
    vnode_put(kernel_vnode);
    return KERN_FAILURE;
}


mach_vm_address_t
solve_kernel_symbol(struct kernel_info *kinfo, char *symbol_to_solve)
{
    struct nlist_64 *nlist = NULL;
    mach_vm_address_t  uValue = 0;
    
    if (kinfo == NULL || kinfo->linkedit_buf == NULL) return 0;
    
    mach_vm_address_t symbol_off = kinfo->symboltable_fileoff - kinfo->linkedit_fileoff;
    mach_vm_address_t string_off = kinfo->stringtable_fileoff - kinfo->linkedit_fileoff;
    
    for (uint32_t i = 0; i < kinfo->symboltable_nr_symbols; i++)
    {
        nlist = (struct nlist_64*)((char*)kinfo->linkedit_buf + symbol_off + i * sizeof(struct nlist_64));
        char *symbol_string = ((char*)kinfo->linkedit_buf + string_off + nlist->n_un.n_strx);
        //kernel_print_log(symbol_string);
        if (strncmp(symbol_to_solve, symbol_string, strlen(symbol_to_solve)) == 0
            && strncmp(symbol_string, symbol_to_solve, strlen(symbol_string)) == 0)
        {
            
            uValue  = nlist->n_value + kinfo->kaslr_slide;
            return ((unsigned long long)uValue);
        }
    }
    return 0;
    
}

/*
 * return the address of the symbol after the one in the parameter
 * this is a cheap/not very reliable trick to find out the size of a given symbol
 * cheap because we might have static functions between the two symbols, for example
 */
mach_vm_address_t
solve_next_kernel_symbol(const struct kernel_info *kinfo, const char *symbol)
{
    struct nlist_64 *nlist = NULL;
    
    if (kinfo == NULL || kinfo->linkedit_buf == NULL) return 0;
    
    mach_vm_address_t symbol_off = kinfo->symboltable_fileoff - kinfo->linkedit_fileoff;
    mach_vm_address_t string_off = kinfo->stringtable_fileoff - kinfo->linkedit_fileoff;
    
    for (uint32_t i = 0; i < kinfo->symboltable_nr_symbols; i++)
    {
        nlist = (struct nlist_64*)((char*)kinfo->linkedit_buf + symbol_off + i * sizeof(struct nlist_64));
        char *symbol_string = ((char*)kinfo->linkedit_buf + string_off + nlist->n_un.n_strx);
        if (strncmp(symbol, symbol_string, strlen(symbol)) == 0)
        {
            // lookup the next symbol
            nlist = (struct nlist_64*)((char*)kinfo->linkedit_buf + symbol_off + (i+1) * sizeof(struct nlist_64));
            symbol_string = ((char*)kinfo->linkedit_buf + string_off + nlist->n_un.n_strx);
#if DEBUG
            //moony_modify//printf("[DEBUG] found next symbol %s at %llx (%s)\n", symbol, nlist->n_value, symbol_string);
#endif
            return (nlist->n_value + kinfo->kaslr_slide);
        }
    }
    // failure
    return 0;
}


/*
 * retrieve the first page of kernel binary at disk into a buffer
 * version that uses KPI VFS functions and a ripped uio_createwithbuffer() from XNU
 */
static kern_return_t
get_kernel_mach_header(void *buffer, vnode_t kernel_vnode, vfs_context_t vfs)
{
    int error = 0;
    
    uio_t uio = NULL;
    //    char uio_buf[UIO_SIZEOF(1)];
    //    uio = uio_createwithbuffer(1, 0, UIO_SYSSPACE, UIO_READ, &uio_buf[0], sizeof(uio_buf));
    uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
    if (uio == NULL) return KERN_FAILURE;
    // imitate the kernel and read a single page from the header
    error = uio_addiov(uio, CAST_USER_ADDR_T(buffer), PAGE_SIZE_64);
    if (error) return error;
    // read kernel vnode into the buffer
    error = VNOP_READ(kernel_vnode, uio, 0, vfs);
    
    if (error) return error;
    else if (uio_resid(uio)) return EINVAL;
    
    return KERN_SUCCESS;
}

/*
 * retrieve the whole linkedit segment into target buffer from kernel binary at disk
 * we keep this buffer until we don't need to solve symbols anymore
 */
static kern_return_t
get_kernel_linkedit(vnode_t kernel_vnode,vfs_context_t vfs, struct kernel_info *kinfo)
{
    int error = 0;
    uio_t uio = NULL;
    uio = uio_create(1, kinfo->linkedit_fileoff, UIO_SYSSPACE, UIO_READ);
    if (uio == NULL) return KERN_FAILURE;
    error = uio_addiov(uio, CAST_USER_ADDR_T(kinfo->linkedit_buf), kinfo->linkedit_size);
    if (error) return error;
    error = VNOP_READ(kernel_vnode, uio, 0, vfs);
    
    if (error) return error;
    else if (uio_resid(uio)) return EINVAL;
    
    return KERN_SUCCESS;
}

/*
 * retrieve necessary mach-o header information from the kernel buffer
 * stored at our kernel_info structure
 */
static kern_return_t
process_kernel_mach_header(void *kernel_header, struct kernel_info *kinfo)
{
    struct mach_header_64 *mh = (struct mach_header_64*)kernel_header;
    // test if it's a valid mach-o header (or appears to be)
    if (mh->magic != MH_MAGIC_64) return KERN_FAILURE;
    
    struct load_command *load_cmd = NULL;
    // point to the first load command
    char *load_cmd_addr = (char*)kernel_header + sizeof(struct mach_header_64);
    // iterate over all load cmds and retrieve required info to solve symbols
    // __LINKEDIT location and symbol/string table location
    for (uint32_t i = 0; i < mh->ncmds; i++)
    {
        load_cmd = (struct load_command*)load_cmd_addr;
        if (load_cmd->cmd == LC_SEGMENT_64)
        {
            struct segment_command_64 *seg_cmd = (struct segment_command_64*)load_cmd;
            // use this one to retrieve the original vm address of __TEXT so we can compute kernel aslr slide
            if (strncmp(seg_cmd->segname, "__TEXT", 16) == 0)
            {
                kinfo->disk_text_addr = seg_cmd->vmaddr;
                // lookup the __text section - we want the size which can be retrieve here or from the running version
                char *section_addr = load_cmd_addr + sizeof(struct segment_command_64);
                struct section_64 *section_cmd = NULL;
                // iterate thru all sections
                for (uint32_t x = 0; x < seg_cmd->nsects; x++)
                {
                    section_cmd = (struct section_64*)section_addr;
                    if (strncmp(section_cmd->sectname, "__text", 16) == 0)
                    {
                        kinfo->text_size = section_cmd->size;
                        break;
                    }
                    section_addr += sizeof(struct section_64);
                }
            }
            else if (strncmp(seg_cmd->segname, "__LINKEDIT", 16) == 0)
            {
                kinfo->linkedit_fileoff = seg_cmd->fileoff;
                kinfo->linkedit_size    = seg_cmd->filesize;
            }
            
            else if (strncmp(seg_cmd->segname, "__DATA", 16) == 0)
            {
                
                kinfo->disk_data_addr = seg_cmd->fileoff;
                kinfo->data_size   = seg_cmd->filesize;
                kinfo->disk_data_vm = seg_cmd->vmaddr;
                kinfo->data_vmsize = seg_cmd->vmsize;
            }
        }
        // table information available at LC_SYMTAB command
        else if (load_cmd->cmd == LC_SYMTAB)
        {
            struct symtab_command *symtab_cmd = (struct symtab_command*)load_cmd;
            kinfo->symboltable_fileoff    = symtab_cmd->symoff;
            kinfo->symboltable_nr_symbols = symtab_cmd->nsyms;
            kinfo->stringtable_fileoff    = symtab_cmd->stroff;
            kinfo->stringtable_size       = symtab_cmd->strsize;
        }
        load_cmd_addr += load_cmd->cmdsize;
    }
    return KERN_SUCCESS;
}

static void
get_running_kernel_address(struct kernel_info *kinfo)
{
    kinfo->running_text_addr = kinfo->disk_text_addr + kinfo->kaslr_slide;
    kinfo->running_kernel_base_addr = 0;
}

static mach_vm_address_t
find_kernel_kaslr() {
    vm_offset_t g_kernel_slide = 0;
    vm_offset_t func_address = (vm_offset_t) vm_kernel_unslide_or_perm_external;
    vm_offset_t func_address_unslid = 0;
    vm_kernel_unslide_or_perm_external(func_address, &func_address_unslid);
    g_kernel_slide = func_address - func_address_unslid;
    return g_kernel_slide;
}

kern_return_t
cleanup_kernel_info(struct kernel_info *kinfo)
{
    if (kinfo->linkedit_buf != NULL)
    {
        _FREE(kinfo->linkedit_buf, M_TEMP);
        kinfo->linkedit_buf = NULL;
    }
    return KERN_SUCCESS;
}
