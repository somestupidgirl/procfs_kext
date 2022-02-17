//
//  cpu_protection.h
//  makedebugpoint
//
//  Created by zuff on 2019/3/4.
//  Copyright © 2019 zuff. All rights reserved.
//

#ifndef cpu_protection_h
#define cpu_protection_h


#define enable_interrupts() __asm__ volatile("sti");
#define disable_interrupts() __asm__ volatile("cli");


#include <i386/proc_reg.h>

uint8_t disable_wp(void);
uint8_t enable_wp(void);
uint8_t verify_wp(void);


#endif /* cpu_protection_h */
