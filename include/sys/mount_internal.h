/*
 * Copyright (c) 2000-2018 Apple Inc. All rights reserved.
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
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mount.h	8.21 (Berkeley) 5/20/95
 */
/*
 * NOTICE: This file was modified by McAfee Research in 2004 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

#ifndef _SYS_MOUNT_INTERNAL_H_
#define _SYS_MOUNT_INTERNAL_H_

#include <sys/appleapiopts.h>
#include <sys/kernel_types.h>
#include <sys/namei.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <net/radix.h>
#include <sys/socket.h>         /* XXX for AF_MAX */
#include <sys/mount.h>
#include <sys/cdefs.h>
#include <sys/sysctl.h>

struct label;

#if defined(__i386__) || defined(__x86_64__)
typedef uint64_t  pending_io_t;
#define INCR_PENDING_IO(a, b) OSAddAtomic64((int64_t)(a), (int64_t *)&(b));
#else
typedef uint32_t  pending_io_t;
#define INCR_PENDING_IO(a, b) OSAddAtomic((int32_t)(a), (int32_t *)&(b));
#endif

/*
 * Structure per mounted file system.  Each mounted file system has an
 * array of operations and an instance record.  The file systems are
 * put on a doubly linked list.
 */
TAILQ_HEAD(vnodelst, vnode);

struct mount {
	TAILQ_ENTRY(mount)      mnt_list;                   /* mount list */
	int32_t                 mnt_count;                  /* reference on the mount */
	lck_mtx_t               mnt_mlock;                  /* mutex that protects mount point */
	const struct vfsops     *mnt_op;        /* operations on fs */
	struct vfstable         *mnt_vtable;        /* configuration info */
	struct vnode            *mnt_vnodecovered;    /* vnode we mounted on */
	struct vnodelst         mnt_vnodelist;              /* list of vnodes this mount */
	struct vnodelst         mnt_workerqueue;            /* list of vnodes this mount */
	struct vnodelst         mnt_newvnodes;              /* list of vnodes this mount */
	uint32_t                mnt_flag;                   /* flags */
	uint32_t                mnt_kern_flag;              /* kernel only flags.  NOTE: See mnt_supl_kern_flags below! */
	uint32_t                mnt_compound_ops;           /* Available compound operations */
	uint32_t                mnt_lflag;                  /* mount life cycle flags */
	uint32_t                mnt_maxsymlinklen;          /* max size of short symlink */
	struct vfsstatfs        mnt_vfsstat;                /* cache of filesystem stats */
	qaddr_t                 mnt_data;                   /* private data */
	/* Cached values of the IO constraints for the device */
	uint32_t                mnt_maxreadcnt;             /* Max. byte count for read */
	uint32_t                mnt_maxwritecnt;            /* Max. byte count for write */
	uint32_t                mnt_segreadcnt;             /* Max. segment count for read */
	uint32_t                mnt_segwritecnt;            /* Max. segment count for write */
	uint32_t                mnt_maxsegreadsize;         /* Max. segment read size  */
	uint32_t                mnt_maxsegwritesize;        /* Max. segment write size */
	uint32_t                mnt_alignmentmask;          /* Mask of bits that aren't addressable via DMA */
	uint32_t                mnt_devblocksize;           /* the underlying device block size */
	uint32_t                mnt_ioqueue_depth;          /* the maxiumum number of commands a device can accept */
	uint32_t                mnt_ioscale;                /* scale the various throttles/limits imposed on the amount of I/O in flight */
	uint32_t                mnt_ioflags;                /* flags for  underlying device */
	uint32_t                mnt_minsaturationbytecount; /* if non-zero, mininum amount of writes (in bytes) needed to max out throughput */
	pending_io_t            mnt_pending_write_size __attribute__((aligned(sizeof(pending_io_t))));  /* byte count of pending writes */
	pending_io_t            mnt_pending_read_size  __attribute__((aligned(sizeof(pending_io_t))));  /* byte count of pending reads */
	struct timeval          mnt_last_write_issued_timestamp;
	struct timeval          mnt_last_write_completed_timestamp;
	int64_t                 mnt_max_swappin_available;

	lck_rw_t                mnt_rwlock;                 /* mutex readwrite lock */
	lck_mtx_t               mnt_renamelock;             /* mutex that serializes renames that change shape of tree */
	vnode_t                 mnt_devvp;                  /* the device mounted on for local file systems */
	uint32_t                mnt_devbsdunit;             /* the BSD unit number of the device */
	uint64_t                mnt_throttle_mask;          /* the throttle mask of what devices will be affected by I/O from this mnt */
	void                    *mnt_throttle_info;         /* used by the throttle code */
	int32_t                 mnt_crossref;               /* refernces to cover lookups  crossing into mp */
	int32_t                 mnt_iterref;                /* refernces to cover iterations; drained makes it -ve  */
#if CONFIG_TRIGGERS
	int32_t                 mnt_numtriggers;            /* num of trigger vnodes for this mount */
	vfs_trigger_callback_t  *mnt_triggercallback;
	void                    *mnt_triggerdata;
#endif
	/* XXX 3762912 hack to support HFS filesystem 'owner' */
	uid_t                   mnt_fsowner;
	gid_t                   mnt_fsgroup;

	struct label            *mnt_mntlabel;    /* MAC mount label */
	struct label            *mnt_fslabel;      /* MAC default fs label */

	/*
	 * cache the rootvp of the last mount point
	 * in the chain in the mount struct pointed
	 * to by the vnode sitting in '/'
	 * this cache is used to shortcircuit the
	 * mount chain traversal and allows us
	 * to traverse to the true underlying rootvp
	 * in 1 easy step inside of 'cache_lookup_path'
	 *
	 * make sure to validate against the cached vid
	 * in case the rootvp gets stolen away since
	 * we don't take an explicit long term reference
	 * on it when we mount it
	 */
	vnode_t                 mnt_realrootvp;
	uint32_t                mnt_realrootvp_vid;
	/*
	 * bumped each time a mount or unmount
	 * occurs... its used to invalidate
	 * 'mnt_realrootvp' from the cache
	 */
	uint32_t                mnt_generation;
	/*
	 * if 'MNTK_AUTH_CACHE_TIMEOUT' is
	 * set, then 'mnt_authcache_ttl' is
	 * the time-to-live for the per-vnode authentication cache
	 * on this mount... if zero, no cache is maintained...
	 * if 'MNTK_AUTH_CACHE_TIMEOUT' isn't set, its the
	 * time-to-live for the cached lookup right for
	 * volumes marked 'MNTK_AUTH_OPAQUE'.
	 */
	int                     mnt_authcache_ttl;
	char                    fstypename_override[MFSTYPENAMELEN];

	uint32_t                mnt_iobufinuse;

	void                    *mnt_disk_conditioner_info;

	lck_mtx_t               mnt_iter_lock;              /* mutex that protects iteration of vnodes */

	uint64_t                mnt_mount_id;               /* system-wide unique mount ID */
	uint32_t                mnt_supl_kern_flag;         /* Supplemental kernel-only mount flags */
};

#endif /* !_SYS_MOUNT_INTERNAL_H_ */
