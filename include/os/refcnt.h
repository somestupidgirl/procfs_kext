/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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

#ifndef _OS_REFCNT_H_
#define _OS_REFCNT_H_

/*
 * os_refcnt reference counting API
 *
 * Two flavors are provided: atomic and locked. Atomic internally uses C11 atomic
 * operations and requires no external synchronization, whereas the locked flavor
 * assumes the refcnt object is locked by the caller. It is NOT safe to
 * mix-and-match locked and atomic calls.
 *
 * 'refgrp's are a way to (hierarchically) group like refcount objects for
 * debugging purposes. The group keeps track of the total number and aggregate
 * reference count of member refcounts, and the "rlog=" boot-arg is used to enable
 * refcount logging by group name. Named groups can be created explicitly with
 * os_refgrp_decl(), or implicitly by passing NULL for the refgrp when
 * initializing a refcnt object. In the latter case, the group name is the same as
 * the function enclosing the init call. Groups are only available on DEV or DEBUG
 * builds, and are otherwise compiled out.
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <os/base.h>

struct os_refcnt;
struct os_refgrp;
typedef struct os_refcnt os_refcnt_t;

/* type of the internal counter */
typedef uint32_t os_ref_count_t;
typedef _Atomic(os_ref_count_t) os_ref_atomic_t;

/*
 * OS_REF_INITIALIZER
 * OS_REF_ATOMIC_INITIALIZER
 *
 * Static initializers that create refcnt objects with safe initial values for use
 * between declaration and initialization (os_ref*_init()). Equivalent to zeroing.
 */


#ifndef OS_REFCNT_DEBUG
# if DEVELOPMENT || DEBUG
#  define OS_REFCNT_DEBUG 1
# else
#  define OS_REFCNT_DEBUG 0
# endif
#endif

#if __has_attribute(diagnose_if)
# define os_error_if(cond, msg) __attribute__((diagnose_if((cond), (msg), "error")))
#else
# define os_error_if(...)
#endif

#endif
