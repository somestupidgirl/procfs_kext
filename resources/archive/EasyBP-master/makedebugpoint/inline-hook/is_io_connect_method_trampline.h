//
//  is_io_connect_method_trampline.h
//  makedebugpoint
//
//  Created by zuff on 2019/3/1.
//  Copyright © 2019 zuff. All rights reserved.
//

#ifndef is_io_connect_method_trampline_h
#define is_io_connect_method_trampline_h

#include <mach/kern_return.h>
#include <sys/types.h>
#include <mach/mach_vm.h>
#include <libkern/libkern.h>
#include <IOKit/IOTypes.h>
#include <device/device_types.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/vnode.h>
#include <string.h>
#include <sys/attr.h>


#define SELECTOR_MASK 0xFEFEF000

#define IS_IO_CONNECT_METHOD_ARGS \
io_connect_t connection, \
uint32_t selector, \
io_scalar_inband64_t scalar_input, \
mach_msg_type_number_t scalar_inputCnt,\
io_struct_inband_t inband_input,\
mach_msg_type_number_t inband_inputCnt,\
mach_vm_address_t ool_input,\
mach_vm_size_t ool_input_size,\
io_struct_inband_t inband_output,\
mach_msg_type_number_t *inband_outputCnt,\
io_scalar_inband64_t scalar_output,\
mach_msg_type_number_t *scalar_outputCnt,\
mach_vm_address_t ool_output,\
mach_vm_size_t *ool_output_size

#define IS_IO_CONNECT_METHOD_ARGS_VAR_LIST \
connection, \
selector,  \
scalar_input, \
scalar_inputCnt,\
inband_input,\
inband_inputCnt,\
ool_input,\
ool_input_size,\
inband_output,\
inband_outputCnt,\
scalar_output,\
scalar_outputCnt,\
ool_output,\
ool_output_size


kern_return_t trampline_is_io_connect_method
(
 struct OSObject * connection,
 //io_connect_t connection,
 uint32_t selector,
 io_scalar_inband64_t scalar_input,
 mach_msg_type_number_t scalar_inputCnt,
 io_struct_inband_t inband_input,
 mach_msg_type_number_t inband_inputCnt,
 mach_vm_address_t ool_input,
 mach_vm_size_t ool_input_size,
 io_struct_inband_t inband_output,
 mach_msg_type_number_t *inband_outputCnt,
 io_scalar_inband64_t scalar_output,
 mach_msg_type_number_t *scalar_outputCnt,
 mach_vm_address_t ool_output,
 mach_vm_size_t *ool_output_size
 );


/**/
__attribute__ ((naked)) void inlined_part_is_io_connect_method(void);


typedef kern_return_t (* fn_is_io_connect_method_t)
(
 struct OSObject * connection,
 //io_connect_t connection,
 uint32_t selector,
 io_scalar_inband64_t scalar_input,
 mach_msg_type_number_t scalar_inputCnt,
 io_struct_inband_t inband_input,
 mach_msg_type_number_t inband_inputCnt,
 mach_vm_address_t ool_input,
 mach_vm_size_t ool_input_size,
 io_struct_inband_t inband_output,
 mach_msg_type_number_t *inband_outputCnt,
 io_scalar_inband64_t scalar_output,
 mach_msg_type_number_t *scalar_outputCnt,
 mach_vm_address_t ool_output,
 mach_vm_size_t *ool_output_size
 );

kern_return_t init_mutext_for_fuzz_sample(void);
kern_return_t un_init_mutext_for_fuzz_sample(void);


#endif /* is_io_connect_method_trampline_h */
