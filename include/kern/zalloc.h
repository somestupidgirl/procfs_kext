/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	zalloc.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	 1985
 *
 */


#ifndef _KERN_ZALLOC_H_
#define _KERN_ZALLOC_H_

#include <mach/machine/vm_types.h>
#include <mach_debug/zone_info.h>
#include <kern/kern_types.h>
#include <sys/cdefs.h>

#define __zalloc_deprecated(msg)

typedef struct zone *zone_t;

__BEGIN_DECLS

/*!
 * @typedef zone_id_t
 *
 * @abstract
 * The type for a zone ID.
 */
typedef uint16_t zone_id_t;

/**
 * @enum zone_create_flags_t
 *
 * @abstract
 * Set of flags to pass to zone_create().
 *
 * @discussion
 * Some kernel-wide policies affect all possible created zones.
 * Explicit @c ZC_* win over such policies.
 */
__options_decl(zone_create_flags_t, uint64_t, {
	/** The default value to pass to zone_create() */
	ZC_NONE                 = 0x00000000,

	/** Force the created zone to use VA sequestering */
	ZC_SEQUESTER            = 0x00000001,
	/** Force the created zone @b NOT to use VA sequestering */
	ZC_NOSEQUESTER          = 0x00000002,

	/** Enable per-CPU zone caching for this zone */
	ZC_CACHING              = 0x00000010,
	/** Disable per-CPU zone caching for this zone */
	ZC_NOCACHING            = 0x00000020,


	/** Mark zone as a per-cpu zone */
	ZC_PERCPU               = 0x01000000,

	/** Force the created zone to clear every allocation on free */
	ZC_ZFREE_CLEARMEM       = 0x02000000,

	/** Mark zone as non collectable by zone_gc() */
	ZC_NOGC                 = 0x04000000,

	/** Do not encrypt this zone during hibernation */
	ZC_NOENCRYPT            = 0x08000000,

	/** Type requires alignment to be preserved */
	ZC_ALIGNMENT_REQUIRED   = 0x10000000,

	/** Do not track this zone when gzalloc is engaged */
	ZC_NOGZALLOC            = 0x20000000,

	/** Don't asynchronously replenish the zone via callouts */
	ZC_NOCALLOUT            = 0x40000000,

	/** Can be zdestroy()ed, not default unlike zinit() */
	ZC_DESTRUCTIBLE         = 0x80000000,

});

/*!
 * @union zone_or_view
 *
 * @abstract
 * A type used for calls that admit both a zone or a zone view.
 *
 * @discussion
 * @c zalloc() and @c zfree() and their variants can act on both
 * zones and zone views.
 */
union zone_or_view {
	struct zone_view *zov_view;
	struct zone      *zov_zone;
#ifdef __cplusplus
	inline zone_or_view(struct zone_view *zv) : zov_view(zv) {
	}
	inline zone_or_view(struct zone *z) : zov_zone(z) {
	}
#endif
};
#ifdef __cplusplus
typedef union zone_or_view zone_or_view_t;
#else
typedef union zone_or_view zone_or_view_t __attribute__((transparent_union));
#endif

/*!
 * @function zone_create
 *
 * @abstract
 * Creates a zone with the specified parameters.
 *
 * @discussion
 * A Zone is a slab allocator that returns objects of a given size very quickly.
 *
 * @param name          the name for the new zone.
 * @param size          the size of the elements returned by this zone.
 * @param flags         a set of @c zone_create_flags_t flags.
 *
 * @returns             the created zone, this call never fails.
 */
extern zone_t   zone_create(
	const char             *name,
	vm_size_t               size,
	zone_create_flags_t     flags);

/*!
 * @function zdestroy
 *
 * @abstract
 * Destroys a zone previously made with zone_create.
 *
 * @discussion
 * Zones must have been made destructible for @c zdestroy() to be allowed,
 * passing @c ZC_DESTRUCTIBLE at @c zone_create() time.
 *
 * @param zone          the zone to destroy.
 */
extern void     zdestroy(
	zone_t          zone);

/*!
 * @function zone_require
 *
 * @abstract
 * Requires for a given pointer to belong to the specified zone.
 *
 * @discussion
 * The function panics if the check fails as it indicates that the kernel
 * internals have been compromised.
 *
 * Note that zone_require() can only work with:
 * - zones not allowing foreign memory
 * - zones in the general submap.
 *
 * @param zone          the zone the address needs to belong to.
 * @param addr          the element address to check.
 */
extern void     zone_require(
	zone_t          zone,
	void           *addr);

/*!
 * @enum zalloc_flags_t
 *
 * @brief
 * Flags that can be passed to @c zalloc_internal or @c zalloc_flags.
 *
 * @discussion
 * It is encouraged that any callsite passing flags uses exactly one of:
 * @c Z_WAITOK, @c Z_NOWAIT or @c Z_NOPAGEWAIT, the default being @c Z_WAITOK
 * if nothing else was specified.
 *
 * If any @c Z_NO*WAIT flag is passed alongside @c Z_WAITOK,
 * then @c Z_WAITOK is ignored.
 *
 * @const Z_WAITOK
 * Means that it's OK for zalloc() to block to wait for memory,
 * when Z_WAITOK is passed, zalloc will never return NULL.
 *
 * @const Z_NOWAIT
 * Passing this flag means that zalloc is not allowed to ever block.
 *
 * @const Z_NOPAGEWAIT
 * Passing this flag means that zalloc is allowed to wait due to lock
 * contention, but will not wait for the VM to wait for pages when
 * under memory pressure.
 *
 * @const Z_ZERO
 * Passing this flags means that the returned memory has been zeroed out.
 *
 * @const Z_NOFAIL
 * Passing this flag means that the caller expects the allocation to always
 * succeed. This will result in a panic if this assumption isn't correct.
 *
 * This flag is incompatible with @c Z_NOWAIT or @c Z_NOPAGEWAIT. It also can't
 * be used on exhaustible zones.
 *
 #if XNU_KERNEL_PRIVATE
 *
 * @const Z_VM_TAG_MASK
 * Represents bits in which a vm_tag_t for the allocation can be passed.
 * (used by kalloc for the zone tagging debugging feature).
 #endif
 */
__options_decl(zalloc_flags_t, uint32_t, {
	// values smaller than 0xff are shared with the M_* flags from BSD MALLOC
	Z_WAITOK        = 0x0000,
	Z_NOWAIT        = 0x0001,
	Z_NOPAGEWAIT    = 0x0002,
	Z_ZERO          = 0x0004,

	Z_NOFAIL        = 0x8000,
});

/*!
 * @function zalloc
 *
 * @abstract
 * Allocates an element from a specified zone.
 *
 * @discussion
 * If the zone isn't exhaustible and is expandable, this call never fails.
 *
 * @param zone_or_view  the zone or zone view to allocate from
 *
 * @returns             NULL or the allocated element
 */
extern void    *zalloc(
	zone_or_view_t  zone_or_view);

/*!
 * @function zalloc_noblock
 *
 * @abstract
 * Allocates an element from a specified zone, but never blocks.
 *
 * @discussion
 * This call is suitable for preemptible code, however allocation
 * isn't allowed from interrupt context.
 *
 * @param zone_or_view  the zone or zone view to allocate from
 *
 * @returns             NULL or the allocated element
 */
extern void    *zalloc_noblock(
	zone_or_view_t  zone_or_view);

/*!
 * @function zalloc_flags()
 *
 * @abstract
 * Allocates an element from a specified zone, with flags.
 *
 * @param zone_or_view  the zone or zone view to allocate from
 * @param flags         a collection of @c zalloc_flags_t.
 *
 * @returns             NULL or the allocated element
 */
extern void    *zalloc_flags(
	zone_or_view_t  zone_or_view,
	zalloc_flags_t  flags);

/*!
 * @function zfree
 *
 * @abstract
 * Frees an element allocated with @c zalloc*.
 *
 * @discussion
 * If the element being freed doesn't belong to the specified zone,
 * then this call will panic.
 *
 * @param zone_or_view  the zone or zone view to free the element to.
 * @param elem          the element to free
 */
extern void     zfree(
	zone_or_view_t  zone_or_view,
	void            *elem);

/* deprecated KPIS */

__zalloc_deprecated("use zone_create()")
extern zone_t   zinit(
	vm_size_t       size,           /* the size of an element */
	vm_size_t       maxmem,         /* maximum memory to use */
	vm_size_t       alloc,          /* allocation size */
	const char      *name);         /* a name for the zone */


__END_DECLS

#endif  /* _KERN_ZALLOC_H_ */

