//
//  kernel_info.h
//  makedebugpoint
//
//  Created by zuff on 2019/3/1.
//  Copyright © 2019 zuff. All rights reserved.
//

#ifndef kernel_info_h
#define kernel_info_h

#include "globle_data_definitions.h"
#include <sys/kernel_types.h>
#include <libkern/libkern.h>




#define KERNEL_HEADER_ADDR 0xffffff8000200000
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;

kern_return_t init_kernel_info(struct kernel_info *kinfo);
kern_return_t cleanup_kernel_info(struct kernel_info *kinfo);


mach_vm_address_t solve_kernel_symbol(struct kernel_info *kinfo, char *symbol_to_solve);


#endif /* kernel_info_h */
