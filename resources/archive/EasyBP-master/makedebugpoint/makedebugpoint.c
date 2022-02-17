//
//  makedebugpoint.c
//  makedebugpoint
//
//  Created by zuff on 2019/3/1.
//  Copyright © 2019 zuff. All rights reserved.
//

#include <mach/mach_types.h>
#include "globle_data_definitions.h"
#include "inline-hook/inline_hook.h"
#include "solve_kernel_symbols/kernel_info.h"



kern_return_t makedebugpoint_start(kmod_info_t * ki, void *d);
kern_return_t makedebugpoint_stop(kmod_info_t *ki, void *d);

// global structures
struct kernel_info g_kernel_info;


kern_return_t makedebugpoint_start(kmod_info_t * ki, void *d)
{
    kern_return_t kr= KERN_SUCCESS;
    //__asm__ volatile ("int3");
    kr = init_kernel_info(&g_kernel_info);
    if(kr) return KERN_FAILURE;
    kr |= init_inline_hook();
    kr |= install_inline_hook();
    
    //__asm__ volatile ("int3");
    return KERN_SUCCESS;
}

kern_return_t makedebugpoint_stop(kmod_info_t *ki, void *d)
{
    un_install_inline_hook();
    un_init_inline_hook();
    cleanup_kernel_info(&g_kernel_info);
    return KERN_SUCCESS;
}
