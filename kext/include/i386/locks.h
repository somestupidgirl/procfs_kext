/*
 * Copyright (c) 2004-2012 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef _I386_LOCKS_H_
#define _I386_LOCKS_H_

#include <sys/appleapiopts.h>
#include <kern/kern_types.h>
#include <kern/assert.h>
#include <mach/mach_types.h>
#include <sys/types.h>

typedef struct {
    volatile uintptr_t      interlock;
    unsigned long           lck_spin_pad[9];        /* XXX - usimple_lock_data_t */
} lck_spin_t;

typedef struct _lck_mtx_ {
    union {
        struct {
            volatile uintptr_t              lck_mtx_owner;
            union {
                struct {
                    volatile uint32_t
                        lck_mtx_waiters:16,
                        lck_mtx_pri:8, // unused
                        lck_mtx_ilocked:1,
                        lck_mtx_mlocked:1,
                        lck_mtx_promoted:1, // unused
                        lck_mtx_spin:1,
                        lck_mtx_is_ext:1,
                        lck_mtx_pad3:3;
                };
                uint32_t        lck_mtx_state;
            };
            /* Pad field used as a canary, initialized to ~0 */
            uint32_t                        lck_mtx_pad32;
        };
        struct {
            struct _lck_mtx_ext_            *lck_mtx_ptr;
            uint32_t                        lck_mtx_tag;
            uint32_t                        lck_mtx_pad32_2;
        };
    };
} lck_mtx_t;

typedef enum lck_mtx_spinwait_ret_type {
    LCK_MTX_SPINWAIT_ACQUIRED = 0,

    LCK_MTX_SPINWAIT_SPUN_HIGH_THR = 1,
    LCK_MTX_SPINWAIT_SPUN_OWNER_NOT_CORE = 2,
    LCK_MTX_SPINWAIT_SPUN_NO_WINDOW_CONTENTION = 3,
    LCK_MTX_SPINWAIT_SPUN_SLIDING_THR = 4,

    LCK_MTX_SPINWAIT_NO_SPIN = 5,
} lck_mtx_spinwait_ret_type_t;

typedef struct {
    unsigned int            type;
    unsigned int            pad4;
    vm_offset_t             pc;
    vm_offset_t             thread;
} lck_mtx_deb_t;

typedef struct {
    unsigned int            lck_mtx_stat_data;
} lck_mtx_stat_t;

typedef struct _lck_mtx_ext_ {
    lck_mtx_t               lck_mtx;
    struct _lck_grp_        *lck_mtx_grp;
    unsigned int            lck_mtx_attr;
    unsigned int            lck_mtx_pad1;
    lck_mtx_deb_t           lck_mtx_deb;
    uint64_t                lck_mtx_stat;
    unsigned int            lck_mtx_pad2[2];
} lck_mtx_ext_t;

typedef union _lck_rw_t_internal_ {
    struct {
        volatile uint16_t       lck_rw_shared_count;    /* No. of accepted readers */
        volatile uint8_t        lck_rw_interlock;       /* Interlock byte */
        volatile uint8_t
            lck_rw_priv_excl:1,                         /* Writers prioritized if set */
            lck_rw_want_upgrade:1,                      /* Read-to-write upgrade waiting */
            lck_rw_want_write:1,                        /* Writer waiting or locked for write */
            lck_r_waiting:1,                            /* Reader is sleeping on lock */
            lck_w_waiting:1,                            /* Writer is sleeping on lock */
            lck_rw_can_sleep:1,                         /* Can attempts to lock go to sleep? */
            lck_rw_padb6:2;                             /* padding */
        uint32_t                lck_rw_tag;             /* This can be obsoleted when stats are in */
        thread_t                lck_rw_owner;           /* Unused */
    };
    struct {
        uint32_t                data;                   /* Single word for count, ilk, and bitfields */
        uint32_t                lck_rw_pad4;
        uint32_t                lck_rw_pad8;
        uint32_t                lck_rw_pad12;
    };
} lck_rw_t;

#endif /* _I386_LOCKS_H_ */