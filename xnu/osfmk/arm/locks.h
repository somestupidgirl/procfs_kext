/*
 * Copyright (c) 2007-2022 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * include/arm/locks.h
 *
 * ARM64 lock type definitions for procfs_kext.
 *
 * The public Kernel.framework SDK only forward-declares these structs
 * in arm/locks.h. The full definitions are sourced from Apple's open
 * source XNU release (xnu-8792.xxx / xnu-10002.xxx) and are required
 * to compile against private headers such as sys/proc_internal.h that
 * embed lock types by value.
 *
 * Source: https://github.com/apple-oss-distributions/xnu
 *         osfmk/arm/locks.h
 */
#ifndef _ARM_LOCKS_H_
#define _ARM_LOCKS_H_

#include <stdint.h>
#include <kern/lock_types.h>

/*
 * Spinlock — ARM64
 *
 * On ARM64 the hardware spinlock uses an exclusive monitor word.
 * The layout matches XNU osfmk/arm/locks.h.
 */
typedef struct __lck_spin_t__ {
    uintptr_t       opaque;
} lck_spin_t;

/*
 * Mutex — ARM64
 *
 * The ARM64 lck_mtx_t is a 16-byte structure. The layout here matches
 * XNU osfmk/arm/locks.h as of macOS Ventura/Sonoma/Sequoia (xnu-8792+).
 *
 * Fields:
 *   lck_mtxd_owner   — current owner thread (pointer-sized)
 *   lck_mtxd_state   — packed state word (waiters, flags)
 *   lck_mtxd_pad8    — padding to 16 bytes
 */
typedef struct __lck_mtx_t__ {
    uintptr_t       lck_mtxd_owner;
    union {
        struct {
            uint16_t    lck_mtxd_waiters;
            uint8_t     lck_mtxd_pri;
            uint8_t     lck_mtxd_ilocked  : 1,
                        lck_mtxd_mlocked  : 1,
                        lck_mtxd_promoted : 1,
                        lck_mtxd_spin     : 1,
                        lck_mtxd_is_ext   : 1,
                        lck_mtxd_pad3     : 3;
        };
        uint32_t        lck_mtxd_state;
    };
    uint32_t            lck_mtxd_pad8;
} lck_mtx_t;

/*
 * Extended mutex — wraps lck_mtx_t with group/debug info.
 */
typedef struct __lck_mtx_ext_t__ {
    lck_mtx_t           lck_mtx;
    struct _lck_grp_   *lck_mtx_grp;
    uint64_t            lck_mtx_stat;
} lck_mtx_ext_t;

/*
 * Read-write lock — ARM64
 *
 * Matches XNU osfmk/arm/locks.h lck_rw_t layout.
 * The structure is 16 bytes on ARM64.
 */
typedef struct __lck_rw_t__ {
    union {
        struct {
            uint16_t    lck_rw_shared_count;
            uint8_t     lck_rw_interlock;
            uint8_t     lck_rw_priv_excl    : 1,
                        lck_rw_want_upgrade  : 1,
                        lck_rw_want_write    : 1,
                        lck_r_waiting        : 1,
                        lck_w_waiting        : 1,
                        lck_rw_can_sleep     : 1,
                        lck_rw_padb6         : 2;
            uint32_t    lck_rw_tag;
            uintptr_t   lck_rw_owner;
        };
        struct {
            uint32_t    data;
            uint32_t    lck_rw_pad4;
            uint64_t    lck_rw_pad8;
        };
    };
} lck_rw_t;

#endif /* _ARM_LOCKS_H_ */
