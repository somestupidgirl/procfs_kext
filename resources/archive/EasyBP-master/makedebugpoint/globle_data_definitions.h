//
//  globle_data_definitions.h
//  makedebugpoint
//
//  Created by zuff on 2019/3/1.
//  Copyright © 2019 zuff. All rights reserved.
//

#ifndef globle_data_definitions_h
#define globle_data_definitions_h

#include <mach/mach_types.h>
#include <sys/types.h>
#include <stdint.h>

struct kernel_info
{
    mach_vm_address_t running_kernel_base_addr; // the address of kernel base
    mach_vm_address_t running_text_addr; // the address of running __TEXT segment
    mach_vm_address_t disk_text_addr;    // the same address at /mach_kernel in filesystem
    mach_vm_address_t kaslr_slide;       // the kernel aslr slide, computed as the difference between above's addresses
    void *linkedit_buf;                  // pointer to __LINKEDIT buffer containing symbols to solve
    uint64_t linkedit_fileoff;           // __LINKEDIT file offset so we can read
    uint64_t linkedit_size;
    uint32_t symboltable_fileoff;        // file offset to symbol table - used to position inside the __LINKEDIT buffer
    uint32_t symboltable_nr_symbols;
    uint32_t stringtable_fileoff;        // file offset to string table
    uint32_t stringtable_size;
    // other info from the header we might need
    uint64_t text_size;                  // size of __text section to disassemble
    
    mach_vm_address_t  disk_data_addr;
    uint64_t data_size;
    mach_vm_address_t  disk_data_vm;
    uint64_t data_vmsize;
    
    struct mach_header_64 *mh;           // ptr to mach-o header of running kernel
};





#endif /* globle_data_definitions_h */
