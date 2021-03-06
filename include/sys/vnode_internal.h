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

#include <kern/locks.h>
#include <sys/appleapiopts.h>
#include <sys/cdefs.h>
#include <sys/event.h>
#include <sys/lock.h>
#include <sys/namei.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vm.h>
#include <sys/vnode.h>
#include <vm/vm_kern.h>

struct lockf;
struct label;

LIST_HEAD(buflists, buf);

#if CONFIG_TRIGGERS
/*
 * VFS Internal (private) trigger vnode resolver info.
 */
struct vnode_resolve {
	lck_mtx_t                               vr_lock;   /* protects vnode_resolve_t fields */
	trigger_vnode_resolve_callback_t        vr_resolve_func;
	trigger_vnode_unresolve_callback_t      vr_unresolve_func;
	trigger_vnode_rearm_callback_t          vr_rearm_func;
	trigger_vnode_reclaim_callback_t        vr_reclaim_func;
	void *                                  vr_data;   /* private data for resolver */
	uint32_t                                vr_flags;
	uint32_t                                vr_lastseq;
};
typedef struct vnode_resolve *vnode_resolve_t;

/* private vr_flags */
#define VNT_RESOLVED        (1UL << 31)
#define VNT_VFS_UNMOUNTED   (1UL << 30)
#define VNT_EXTERNAL        (1UL << 29)

#endif /* CONFIG_TRIGGERS */

/*
 * Reading or writing any of these items requires holding the appropriate lock.
 * v_freelist is locked by the global vnode_list_lock
 * v_mntvnodes is locked by the mount_lock
 * v_nclinks and v_ncchildren are protected by the global name_cache_lock
 * v_cleanblkhd and v_dirtyblkhd and v_iterblkflags are locked via the global buf_mtx
 * the rest of the structure is protected by the vnode_lock
 */
struct vnode {
	lck_mtx_t                  v_lock;                /* vnode mutex */
	TAILQ_ENTRY(vnode)         v_freelist;            /* vnode freelist */
	TAILQ_ENTRY(vnode)         v_mntvnodes;           /* vnodes for mount point */
	TAILQ_HEAD(, namecache)    v_ncchildren;          /* name cache entries that regard us as their parent */
	LIST_HEAD(, namecache)     v_nclinks;             /* name cache entries that name this vnode */
	vnode_t                    v_defer_reclaimlist;   /* in case we have to defer the reclaim to avoid recursion */
	uint32_t                   v_listflag;            /* flags protected by the vnode_list_lock (see below) */
	uint32_t                   v_flag;                /* vnode flags (see below) */
	uint16_t                   v_lflag;               /* vnode local and named ref flags */
	uint8_t                    v_iterblkflags;        /* buf iterator flags */
	uint8_t                    v_references;          /* number of times io_count has been granted */
	int32_t                    v_kusecount;           /* count of in-kernel refs */
	int32_t                    v_usecount;            /* reference count of users */
	int32_t                    v_iocount;             /* iocounters */
	void                      *v_owner;               /* act that owns the vnode */
	uint16_t                   v_type;                /* vnode type */
	uint16_t                   v_tag;                 /* type of underlying data */
	uint32_t                   v_id;                  /* identity of vnode contents */

	union {
		struct mount    *vu_mountedhere;  /* ptr to mounted vfs (VDIR) */
		struct socket   *vu_socket;       /* unix ipc (VSOCK) */
		struct specinfo *vu_specinfo;     /* device (VCHR, VBLK) */
		struct fifoinfo *vu_fifoinfo;     /* fifo (VFIFO) */
		struct ubc_info *vu_ubcinfo;      /* valid for (VREG) */
	} v_un;

	struct buflists            v_cleanblkhd;          /* clean blocklist head */
	struct buflists            v_dirtyblkhd;          /* dirty blocklist head */
	struct klist               v_knotes;              /* knotes attached to this vnode */

	/*
	 * the following 4 fields are protected by the name_cache_lock held in excluive mode.
	 */
	kauth_cred_t               v_cred;                /* last authorized credential */
	kauth_action_t             v_authorized_actions;  /* current authorized actions for v_cred */
	int                        v_cred_timestamp;      /* determine if entry is stale for MNTK_AUTH_OPAQUE */
	int                        v_nc_generation;       /* changes when nodes are removed from the name cache */

	/*
	 * back to the vnode lock for protection
	 */
	int32_t                    v_numoutput;           /* num of writes in progress */
	int32_t                    v_writecount;          /* reference count of writers */
	const char                *v_name;                /* name component of the vnode */
	vnode_t                    v_parent;              /* pointer to parent vnode */
	struct lockf              *v_lockf;               /* advisory lock list head */
	int                     (**v_op)(void *);         /* vnode operations vector */
	mount_t                    v_mount;               /* ptr to vfs we are in */
	void                      *v_data;                /* private data for fs */

#if CONFIG_MACF
	struct label              *v_label;               /* MAC security label */
#endif

#if CONFIG_TRIGGERS
	vnode_resolve_t            v_resolve;             /* trigger vnode resolve info (VDIR only) */
#endif /* CONFIG_TRIGGERS */

#if CONFIG_FIRMLINKS
	vnode_t                    v_fmlink;              /* 
                                                       * firmlink if set (VDIR only), Points to source
	                                                   * if VFLINKTARGET is set, if VFLINKTARGET is not
	                                                   * set, points to target
                                                       */
#endif /* CONFIG_FIRMLINKS */

#if CONFIG_IO_COMPRESSION_STATS
	io_compression_stats_t     io_compression_stats;   /* IO compression statistics */
#endif /* CONFIG_IO_COMPRESSION_STATS */

}; /* struct vnode */

#define v_mountedhere   v_un.vu_mountedhere
#define v_socket        v_un.vu_socket
#define v_specinfo      v_un.vu_specinfo
#define v_fifoinfo      v_un.vu_fifoinfo
#define v_ubcinfo       v_un.vu_ubcinfo

/*
 * v_iterblkflags
 */
#define VBI_ITER                    0x1
#define VBI_ITERWANT                0x2
#define VBI_CLEAN                   0x4
#define VBI_DIRTY                   0x8
#define VBI_NEWBUF                  0x10

/*
 * v_listflag
 */
#define VLIST_RAGE                  0x01            /* vnode is currently in the rapid age list */
#define VLIST_DEAD                  0x02            /* vnode is currently in the dead list */
#define VLIST_ASYNC_WORK            0x04            /* vnode is currently on the deferred async work queue */

/*
 * v_lflags
 */
#define VL_SUSPENDED                0x0001          /* vnode is suspended */
#define VL_DRAIN                    0x0002          /* vnode is being drained */
#define VL_TERMINATE                0x0004          /* vnode is in the process of being recycled */
#define VL_TERMWANT                 0x0008          /* there's a waiter  for recycle finish (vnode_getiocount)*/
#define VL_DEAD                     0x0010          /* vnode is dead, cleaned of filesystem-specific info */
#define VL_MARKTERM                 0x0020          /* vnode should be recycled when no longer referenced */
#define VL_NEEDINACTIVE             0x0080          /* delay VNOP_INACTIVE until iocount goes to 0 */

#define VL_LABEL                    0x0100          /* vnode is marked for labeling */
#define VL_LABELWAIT                0x0200          /* vnode is marked for labeling */
#define VL_LABELED                  0x0400          /* vnode is labeled */
#define VL_LWARNED                  0x0800
#define VL_HASSTREAMS               0x1000          /* vnode has had at least one associated named stream vnode (may not have one any longer) */

#define VNAMED_UBC                  0x2000          /* ubc named reference */
#define VNAMED_MOUNT                0x4000          /* mount point named reference */
#define VNAMED_FSHASH               0x8000          /* FS hash named reference */

/*
 * v_flags
 */
#define VROOT                       0x000001        /* root of its file system */
#define VTEXT                       0x000002        /* vnode is a pure text prototype */
#define VSYSTEM                     0x000004        /* vnode being used by kernel */
#define VISTTY                      0x000008        /* vnode represents a tty */
#define VRAGE                       0x000010        /* vnode is in rapid age state */
#define VBDEVVP                     0x000020        /* vnode created by bdevvp */
#define VDEVFLUSH                   0x000040        /* device vnode after vflush */
#define VMOUNT                      0x000080        /* mount operation in progress */
#define VBWAIT                      0x000100        /* waiting for output to complete */
#define VSHARED_DYLD                0x000200        /* vnode is a dyld shared cache file */
#define VNOCACHE_DATA               0x000400        /* don't keep data cached once it's been consumed */
#define VSTANDARD                   0x000800        /* vnode obtained from common pool */
#define VAGE                        0x001000        /* Insert vnode at head of free list */
#define VRAOFF                      0x002000        /* read ahead disabled */
#define VNCACHEABLE                 0x004000        /* vnode is allowed to be put back in name cache */
#if NAMEDSTREAMS
#define VISSHADOW                   0x008000        /* vnode is a shadow file */
#endif
#define VSWAP                       0x010000        /* vnode is being used as swapfile */
#define VTHROTTLED                  0x020000        /* writes or pageouts have been throttled */
/* wakeup tasks waiting when count falls below threshold */
#define VNOFLUSH                    0x040000        /* don't vflush() if SKIPSYSTEM */
#define VLOCKLOCAL                  0x080000        /* this vnode does adv locking in vfs */
#define VISHARDLINK                 0x100000        /* hard link needs special processing on lookup and in volfs */
#define VISUNION                    0x200000        /* union special processing */
#define VISNAMEDSTREAM              0x400000        /* vnode is a named stream (eg HFS resource fork) */
#define VOPENEVT                    0x800000        /* if process is P_CHECKOPENEVT, then or in the O_EVTONLY flag on open */
#define VNEEDSSNAPSHOT              0x1000000
#define VNOCS                       0x2000000       /* is there no code signature available */
#define VISDIRTY                    0x4000000       /* vnode will need IO if reclaimed */
#define VFASTDEVCANDIDATE           0x8000000       /* vnode is a candidate to store on a fast device */
#define VAUTOCANDIDATE              0x10000000      /* vnode was automatically marked as a fast-dev candidate */
#define VFMLINKTARGET               0x20000000      /* vnode is firmlink target */
/*
 *  0x40000000 not used
 *  0x80000000 not used.
 */

/*
 * This structure describes vnode data which is specific to a file descriptor.
 * It is currently used only for file descriptors which are for opened for
 * directories.
 */
struct fd_vn_data {
	lck_mtx_t fv_lock;         /* Used to serialize directory enumeration on fd */
	off_t     fv_offset;       /* Offset to be used */
	void      *fv_dircookie;   /* If FS doesn't like offsets in directories */
	caddr_t   fv_buf;          /* Temporary buffer to store directory entries */
	size_t    fv_bufsiz;       /* Valid size of fv_buf */
	size_t    fv_bufdone;      /* How much of fv_buf is processed */
	size_t    fv_bufallocsiz;  /* Allocation size determined for Buffer*/
	off_t     fv_soff;         /* Starting FS offset for this buffer */
	off_t     fv_eoff;         /* Ending FS offset for this buffer */
	int       fv_eofflag;      /* Does fv_eoff represent EOF ? */
};

/*
 * FV_DIRBUF_START_SIZ is the initial size of the buffer passed to VNOP_READDIR.
 * That may not be enough for some filesytems so the current algorithm works its
 * way upto FV_DIRBUF_MAX_SIZ
 */
#define FV_DIRBUF_DIRENTRY_SIZ  (sizeof(struct direntry))
#define FV_DIRBUF_START_SIZ     FV_DIRBUF_DIRENTRY_SIZ
#define FV_DIRBUF_MAX_SIZ       (4*(sizeof(struct direntry)))

#define FV_LOCK(fvd)            lck_mtx_lock(&(((struct fd_vn_data *)fvd)->fv_lock))
#define FV_UNLOCK(fvd)          lck_mtx_unlock(&(((struct fd_vn_data *)fvd)->fv_lock))

/*
 * Global vnode data.
 */
extern struct vnode *rootvnode;        /* root (i.e. "/") vnode */

#ifdef CONFIG_IMGSRC_ACCESS
#define MAX_IMAGEBOOT_NESTING   2
extern struct vnode *imgsrc_rootvnodes[];
#endif /* CONFIG_IMGSRC_ACCESS */


/*
 * Mods for exensibility.
 */

/*
 * Flags for vdesc_flags:
 */
#define VDESC_MAX_VPS           16

/* Low order 16 flag bits are reserved for willrele flags for vp arguments. */
#define VDESC_VP0_WILLRELE      0x00001
#define VDESC_VP1_WILLRELE      0x00002
#define VDESC_VP2_WILLRELE      0x00004
#define VDESC_VP3_WILLRELE      0x00008
#define VDESC_NOMAP_VPP         0x00100
#define VDESC_VPP_WILLRELE      0x00200

#define VDESC_DISABLED          0x10000 /* descriptor defined but op is unused, has no op slot */

/*
 * VDESC_NO_OFFSET is used to identify the end of the offset list
 * and in places where no such field exists.
 */
#define VDESC_NO_OFFSET -1

/*
 * This structure describes the vnode operation taking place.
 */
struct vnodeop_desc {
	int            vdesc_offset;               /* offset in vector--first for speed */
	const char    *vdesc_name;                 /* a readable name for debugging */
	int            vdesc_flags;                /* VDESC_* flags */

	/*
	 * These ops are used by bypass routines to map and locate arguments.
	 * Creds and procs are not needed in bypass routines, but sometimes
	 * they are useful to (for example) transport layers.
	 * Nameidata is useful because it has a cred in it.
	 */
	int           *vdesc_vp_offsets;           /* list ended by VDESC_NO_OFFSET */
	int            vdesc_vpp_offset;           /* return vpp location */
	int            vdesc_cred_offset;          /* cred location, if any */
	int            vdesc_proc_offset;          /* proc location, if any */
	int            vdesc_componentname_offset; /* if any */
	int            vdesc_context_offset;       /* context location, if any */
	/*
	 * Finally, we've got a list of private data (about each operation)
	 * for each transport layer.  (Support to manage this list is not
	 * yet part of BSD.)
	 */
	caddr_t       *vdesc_transports;
};

/*
 * A list of all the operation descs.
 */
extern struct vnodeop_desc *vnodeop_descs[];

/*
 * This macro is very helpful in defining those offsets in the vdesc struct.
 *
 * This is stolen from X11R4.  I ingored all the fancy stuff for
 * Crays, so if you decide to port this to such a serious machine,
 * you might want to consult Intrisics.h's XtOffset{,Of,To}.
 */
#define VOPARG_OFFSET(p_type, field) \
	((int) (((char *) (&(((p_type)NULL)->field))) - ((char *) NULL)))
#define VOPARG_OFFSETOF(s_type, field) \
	VOPARG_OFFSET(s_type*,field)
#define VOPARG_OFFSETTO(S_TYPE, S_OFFSET, STRUCT_P) \
	((S_TYPE)(((char*)(STRUCT_P))+(S_OFFSET)))

/*
 * VOCALL calls an op given an ops vector.  We break it out because BSD's
 * vclean changes the ops vector and then wants to call ops with the old
 * vector.
 */
#define VOCALL(OPSV, OFF, AP) (( *((OPSV)[(OFF)])) (AP))

/*
 * This call works for vnodes in the kernel.
 */
#define VCALL(VP, OFF, AP) VOCALL((VP)->v_op,(OFF),(AP))
#define VDESC(OP) (& __CONCAT(OP,_desc))
#define VOFFSET(OP) (VDESC(OP)->vdesc_offset)

#define VN_CREATE_NOAUTH                (1<<0)
#define VN_CREATE_NOINHERIT             (1<<1)
#define VN_CREATE_UNION                 (1<<2)
#define VN_CREATE_NOLABEL               (1<<3)
#define VN_CREATE_DOOPEN                (1<<4)  /* Open file if a batched operation is available */

typedef uint32_t compound_vnop_id_t;
#define COMPOUND_VNOP_OPEN              0x01
#define COMPOUND_VNOP_MKDIR             0x02
#define COMPOUND_VNOP_RENAME            0x04
#define COMPOUND_VNOP_REMOVE            0x08
#define COMPOUND_VNOP_RMDIR             0x10

#endif /* !_SYS_VNODE_INTERNAL_H_ */
