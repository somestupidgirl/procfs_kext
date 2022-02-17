//
//  is_io_connect_method_trampline.c
//  makedebugpoint
//
//  Created by zuff on 2019/3/1.
//  Copyright © 2019 zuff. All rights reserved.
//

#include "is_io_connect_method_trampline.h"
#include "Mutext.h"

#include "inline_hook.h"


static uint64_t s_is_io_connect_method_JmpBackAddr = -1;
extern inline_hook_entry_t g_inline_hook_entry[];


lck_mtx_t *iicm_mutext=NULL;
lck_grp_t *iicm_mutext_group=NULL;



kern_return_t init_mutext_for_fuzz_sample()
{
    return init_mutex(&iicm_mutext, &iicm_mutext_group, "iicm_mutext");
}

kern_return_t un_init_mutext_for_fuzz_sample()
{
    return  un_init_mutex(&iicm_mutext, &iicm_mutext_group);
}

uint32_t refine_selector(uint32_t selector) {
    return selector-SELECTOR_MASK;
}

kern_return_t trampline_is_io_connect_method (IS_IO_CONNECT_METHOD_ARGS)
{
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");
    //__asm__ volatile ("int3");

    s_is_io_connect_method_JmpBackAddr = g_inline_hook_entry[INLINE_ENUM_IS_IO_CONNECT_METHOD].ori_func_addr + TRAMPOLINE_SIZE;
    
    
    //Begin lock
    kern_return_t  kr =0;
    lck_mtx_lock(iicm_mutext);
    
    if (selector >= SELECTOR_MASK)
        //refine selector
    {
        selector = refine_selector(selector);
    }
    
    lck_mtx_unlock(iicm_mutext);
    kr = ((fn_is_io_connect_method_t )inlined_part_is_io_connect_method)(
                                                                         //io_connect_t
                                                                         connection,
                                                                         //uint32_t
                                                                         selector,
                                                                         //io_scalar_inband64_t
                                                                         scalar_input,
                                                                         //mach_msg_type_number_t
                                                                         scalar_inputCnt,
                                                                         //io_struct_inband_t
                                                                         inband_input,
                                                                         //mach_msg_type_number_t
                                                                         inband_inputCnt,
                                                                         //mach_vm_address_t
                                                                         ool_input,
                                                                         //mach_vm_size_t
                                                                         ool_input_size,
                                                                         //io_struct_inband_t
                                                                         inband_output,
                                                                         //mach_msg_type_number_t *
                                                                         inband_outputCnt,
                                                                         //io_scalar_inband64_t
                                                                         scalar_output,
                                                                         //mach_msg_type_number_t *
                                                                         scalar_outputCnt,
                                                                         //mach_vm_address_t
                                                                         ool_output,
                                                                         //mach_vm_size_t *
                                                                         ool_output_size);
    
    return kr;
}


__attribute__ ((naked)) void inlined_part_is_io_connect_method()
{
    __asm__ volatile (
                      "  push %rbp\n"
                      "  mov %rsp, %rbp\n"
                      "  push %r15\n"
                      "  push %r14\n"
                      "  push %r13\n"
                      "  push %r12\n"
                      //"  push %rbx\n"
                      //"  sub $0x108, %rsp"
                      );
    __asm__ volatile (
                      "  jmp *%0\n"
                      //"  mov %%rax, %0"
                      :
                      :"m" (s_is_io_connect_method_JmpBackAddr)
                      //:"%rax"
                      );
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");//10
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");
    __asm__ volatile ("int3");//20
}
