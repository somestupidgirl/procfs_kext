//
//  trampline_manager.c
//  makedebugpoint
//
//  Created by zuff on 2019/3/4.
//  Copyright © 2019 zuff. All rights reserved.
//

#include "trampline_manager.h"


#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <mach-o/loader.h>
#include <string.h>

#include "cpu_protection.h"
#include "configuration.h"

kern_return_t
install_trampoline_any(mach_vm_address_t patch_addr, mach_vm_address_t dest_address, void *orig_bytes)
{
    char trampoline[12] = "\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00" // mov rax, address
    "\xFF\xE0"; // jmp rax
    
    if (patch_addr == 0)
    {
        return KERN_FAILURE;
    }
    
    memcpy(orig_bytes, (void*)patch_addr, sizeof(trampoline));
    // set the target address
    memcpy(trampoline+2, &dest_address, sizeof(mach_vm_address_t));
    // patch the target address with the trampoline
    disable_interrupts();
    disable_wp();
    memcpy((void*)patch_addr, trampoline, sizeof(trampoline));
    enable_wp();
    enable_interrupts();
    return KERN_SUCCESS;
}


kern_return_t
remove_trampoline_any(mach_vm_address_t patch_addr, void *orig_bytes)
{
    //mach_vm_address_t patch_addr = solve_kernel_symbol(&g_kernel_info, symbol);
    if (patch_addr == 0) return KERN_FAILURE;
    
    disable_interrupts();
    disable_wp();
    memcpy((void*)patch_addr, orig_bytes, TRAMPOLINE_SIZE);
    enable_wp();
    enable_interrupts();
    return KERN_SUCCESS;
}
