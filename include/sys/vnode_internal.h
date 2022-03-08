/*
 * Copyright (c) 2000-2012 Apple Inc. All rights reserved.
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
 * Copyright (c) 1989, 1993
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
 *	@(#)vnode.h	8.17 (Berkeley) 5/20/95
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

#ifndef _SYS_VNODE_INTERNAL_H_
#define _SYS_VNODE_INTERNAL_H_

#include <sys/appleapiopts.h>
#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/lock.h>

#include <sys/time.h>
#include <sys/uio.h>

#include <sys/vm.h>
#include <sys/systm.h>
#include <kern/locks.h>
#include <vm/vm_kern.h>
#include <sys/vnode.h>
#include <sys/namei.h>


struct lockf;
struct label;
typedef struct buflists *  buflists_t;
typedef struct klist * klist_t;

/*
 * Reading or writing any of these items requires holding the appropriate lock.
 * v_freelist is locked by the global vnode_list_lock
 * v_mntvnodes is locked by the mount_lock
 * v_nclinks and v_ncchildren are protected by the global name_cache_lock
 * v_cleanblkhd and v_dirtyblkhd and v_iterblkflags are locked via the global buf_mtx
 * the rest of the structure is protected by the vnode_lock
 */
struct vnode {
	lck_mtx_t v_lock;                       /* vnode mutex */
	TAILQ_ENTRY(vnode) v_freelist;          /* vnode freelist */
	TAILQ_ENTRY(vnode) v_mntvnodes;         /* vnodes for mount point */
	TAILQ_HEAD(, namecache) v_ncchildren;   /* name cache entries that regard us as their parent */
	LIST_HEAD(, namecache) v_nclinks;       /* name cache entries that name this vnode */
	vnode_t  v_defer_reclaimlist;           /* in case we have to defer the reclaim to avoid recursion */
	uint32_t v_listflag;                    /* flags protected by the vnode_list_lock (see below) */
	uint32_t v_flag;                        /* vnode flags (see below) */
	uint16_t v_lflag;                       /* vnode local and named ref flags */
	uint8_t  v_iterblkflags;                /* buf iterator flags */
	uint8_t  v_references;                  /* number of times io_count has been granted */
	int32_t  v_kusecount;                   /* count of in-kernel refs */
	int32_t  v_usecount;                    /* reference count of users */
	int32_t  v_iocount;                     /* iocounters */
	void *   v_owner; /* act that owns the vnode */
	uint16_t v_type;                        /* vnode type */
	uint16_t v_tag;                         /* type of underlying data */
	uint32_t v_id;                          /* identity of vnode contents */
	union {
		struct mount    * vu_mountedhere;  /* ptr to mounted vfs (VDIR) */
		struct socket   * vu_socket;     /* unix ipc (VSOCK) */
		struct specinfo * vu_specinfo;   /* device (VCHR, VBLK) */
		struct fifoinfo * vu_fifoinfo;   /* fifo (VFIFO) */
		struct ubc_info * vu_ubcinfo;    /* valid for (VREG) */
	} v_un;
    //struct buflists v_cleanblkhd;          /* clean blocklist head */
    //struct buflists v_dirtyblkhd;          /* dirty blocklist head */
	buflists_t v_cleanblkhd;          /* clean blocklist head */
	buflists_t v_dirtyblkhd;          /* dirty blocklist head */
	//struct klist v_knotes;                  /* knotes attached to this vnode */
    klist_t v_knotes;                  /* knotes attached to this vnode */
	/*
	 * the following 4 fields are protected
	 * by the name_cache_lock held in
	 * excluive mode
	 */
	kauth_cred_t    v_cred; /* last authorized credential */
	kauth_action_t  v_authorized_actions;   /* current authorized actions for v_cred */
	int             v_cred_timestamp;       /* determine if entry is stale for MNTK_AUTH_OPAQUE */
	int             v_nc_generation;        /* changes when nodes are removed from the name cache */
	/*
	 * back to the vnode lock for protection
	 */
	int32_t         v_numoutput;                    /* num of writes in progress */
	int32_t         v_writecount;                   /* reference count of writers */
	const char *v_name;                     /* name component of the vnode */
	vnode_t v_parent;                       /* pointer to parent vnode */
	struct lockf    *v_lockf;               /* advisory lock list head */
	int(**v_op)(void *);                    /* vnode operations vector */
	mount_t v_mount;                        /* ptr to vfs we are in */
	void *  v_data;                         /* private data for fs */
#if CONFIG_MACF
	struct label *v_label;                  /* MAC security label */
#endif
#if CONFIG_TRIGGERS
	vnode_resolve_t v_resolve;              /* trigger vnode resolve info (VDIR only) */
#endif /* CONFIG_TRIGGERS */
#if CONFIG_FIRMLINKS
	vnode_t v_fmlink;                       /* firmlink if set (VDIR only), Points to source
	                                         *  if VFLINKTARGET is set, if  VFLINKTARGET is not
	                                         *  set, points to target */
#endif /* CONFIG_FIRMLINKS */
#if CONFIG_IO_COMPRESSION_STATS
	io_compression_stats_t io_compression_stats;            /* IO compression statistics */
#endif /* CONFIG_IO_COMPRESSION_STATS */
};

#endif /* !_SYS_VNODE_INTERNAL_H_ */
