//
//  sysent.c
//  test
//
//  Created by 五千年木 on 2019/3/19.
//  Copyright © 2019年 osxkernel. All rights reserved.
//

//#include "sysent.h"

#include "sysent.h"
#include <mach/mach_types.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include "version.h"
#include "macho_utils.h"
//#include "test.h"

struct sysent *g_sysent_table = NULL;
section_t* section = NULL;

struct sysent * find_sysent_table()
{
    if (g_sysent_table) {
        return g_sysent_table;
    }
    
    mach_header_t* kernel_header = find_kernel_header();
    
    if (!kernel_header) {
        return NULL;
    }
    
    // The first three entries of the sysent table point to these functions.
    sy_call_t *nosys = (sy_call_t *) kernel_find_symbol("_nosys");
    sy_call_t *exit = (sy_call_t *) kernel_find_symbol("_exit");
    sy_call_t *fork = (sy_call_t *) kernel_find_symbol("_fork");
    if (!nosys || !exit || !fork) {
        LOG_ERROR("moonAgirl[kernel_find_symbol] failed.");
        return NULL;
    }
    LOG_INFO("moonAgirl[kernel_find_symbol] success.");
    
    const char *data_segment_name;
    const char *const_section_name;
    if (macOS_Sierra()) {
        data_segment_name = "__CONST";
        const_section_name = "__constdata";
    } else {
        data_segment_name = "__DATA";
        const_section_name = "__const";
    }
    
    section = macho_find_section(kernel_header, data_segment_name, const_section_name);
    
    if(!section){
        LOG_ERROR("moonAgirl[macho_find_section] failed.");
        return NULL;
    }
    LOG_INFO("moonAgirl[macho_find_section] success.");
    
    vm_offset_t offset;
    for (offset = 0; offset < section->size; offset += 16) {
        struct sysent *table = (struct sysent *) (section->addr + offset);
        if (table->sy_call != nosys) {
            continue;
        }
        vm_offset_t next_entry_offset = sizeof(struct sysent);
        if (OSX_Mavericks()) {
            next_entry_offset = sizeof(struct sysent_mavericks);
        }
        struct sysent *next_entry = (struct sysent *)
        ((vm_offset_t)table + next_entry_offset);
        if (next_entry->sy_call != exit) {
            continue;
        }
        next_entry = (struct sysent *)
        ((vm_offset_t)next_entry + next_entry_offset);
        if (next_entry->sy_call != fork) {
            continue;
        }
        
        g_sysent_table = table;
        return g_sysent_table;
    }
    
    return NULL;
}

static int is_mach_trap_table(uintptr_t addr)
{
#define traps_verify(_traps)                                \
((_traps)[0].mach_trap_arg_count == 0 &&                \
(_traps)[1].mach_trap_arg_count == 0 &&                \
(_traps)[MACH_MSG_TRAP].mach_trap_arg_count == 7 &&    \
(_traps)[MACH_MSG_OVERWRITE_TRAP].mach_trap_arg_count == 8)
    
    //if (version_major >= 13) {
        mach_trap_mavericks_t* res = (mach_trap_mavericks_t*)addr;
        return traps_verify(res);
//    } else {
//        mach_trap_t* res = (mach_trap_t*)addr;
//        return traps_verify(res);
//    }
    
#undef traps_verify
    return FALSE;
}

void* find_mach_tables()
{
    if(!section)return NULL;
    void* mach_traps = NULL;
    
    uintptr_t addr = section->addr;
    uint64_t size = section->size;
    
    while(size != 0) {
        
        if (!mach_traps && is_mach_trap_table(addr)) {
            mach_traps = (void*)addr;
        }
        
        if (mach_traps) {
            return mach_traps;
        }
        
        addr++;
        size--;
    }
    
    return NULL;
}
