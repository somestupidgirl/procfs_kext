//
//  trampline_manager.h
//  makedebugpoint
//
//  Created by zuff on 2019/3/4.
//  Copyright © 2019 zuff. All rights reserved.
//

#ifndef trampline_manager_h
#define trampline_manager_h

#include "globle_data_definitions.h"

kern_return_t install_trampoline_any(mach_vm_address_t patch_addr, mach_vm_address_t dest_address, void *orig_bytes);
kern_return_t remove_trampoline_any(mach_vm_address_t patch_addr, void *orig_bytes);

#endif /* trampline_manager_h */
