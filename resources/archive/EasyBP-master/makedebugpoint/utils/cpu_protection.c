//
//  cpu_protection.c
//  makedebugpoint
//
//  Created by zuff on 2019/3/4.
//  Copyright © 2019 zuff. All rights reserved.
//

#include "cpu_protection.h"



/*
 * disable the Write Protection bit in CR0 register so we can modify kernel code
 */
uint8_t
disable_wp(void)
{
    uintptr_t cr0;
    // retrieve current value
    cr0 = get_cr0();
    // remove the WP bit
    cr0 = cr0 & ~CR0_WP;
    // and write it back
    set_cr0(cr0);
    // verify if we were successful
    if ((get_cr0() & CR0_WP) == 0) return 0;
    else return 1;
}

/*
 * enable the Write Protection bit in CR0 register
 */
uint8_t
enable_wp(void)
{
    uintptr_t cr0;
    // retrieve current value
    cr0 = get_cr0();
    // add the WP bit
    cr0 = cr0 | CR0_WP;
    // and write it back
    set_cr0(cr0);
    // verify if we were successful
    if ((get_cr0() & CR0_WP) != 0) return 0;
    else return 1;
}

/*
 * check if WP is set or not
 * 0 - it's set
 * 1 - not set
 */
uint8_t
verify_wp(void)
{
    uintptr_t cr0;
    cr0 = get_cr0();
    if (cr0 & CR0_WP) return 0;
    else return 1;
}
