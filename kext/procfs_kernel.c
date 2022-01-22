#include <stdatomic.h>

#include <kern/thread.h>

#include <mach/message.h>
#include <mach/task.h>
#include <mach/thread_info.h>
#include <mach/i386/kern_return.h>

#include <os/refcnt.h>

#include <sys/errno.h>
#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/guarded.h>
#include <sys/mount.h>
#include <sys/mount_internal.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/vnode_internal.h>

#include "procfs_kernel.h"
#include "procfs_locks.h"


/*
 * ======= fill_vnodeinfo() =======
 * From XNU: bsd/kern/proc_info.c
 * Status: Mostly Compiling
 */

static void munge_vinfo_stat(struct stat64 *sbp, struct vinfo_stat *vsbp);

struct mount * dead_mountp;

/*
 * copy stat64 structure into vinfo_stat structure.
 */
static void
munge_vinfo_stat(struct stat64 *sbp, struct vinfo_stat *vsbp)
{
	bzero(vsbp, sizeof(struct vinfo_stat));

	vsbp->vst_dev = sbp->st_dev;
	vsbp->vst_mode = sbp->st_mode;
	vsbp->vst_nlink = sbp->st_nlink;
	vsbp->vst_ino = sbp->st_ino;
	vsbp->vst_uid = sbp->st_uid;
	vsbp->vst_gid = sbp->st_gid;
	vsbp->vst_atime = sbp->st_atimespec.tv_sec;
	vsbp->vst_atimensec = sbp->st_atimespec.tv_nsec;
	vsbp->vst_mtime = sbp->st_mtimespec.tv_sec;
	vsbp->vst_mtimensec = sbp->st_mtimespec.tv_nsec;
	vsbp->vst_ctime = sbp->st_ctimespec.tv_sec;
	vsbp->vst_ctimensec = sbp->st_ctimespec.tv_nsec;
	vsbp->vst_birthtime = sbp->st_birthtimespec.tv_sec;
	vsbp->vst_birthtimensec = sbp->st_birthtimespec.tv_nsec;
	vsbp->vst_size = sbp->st_size;
	vsbp->vst_blocks = sbp->st_blocks;
	vsbp->vst_blksize = sbp->st_blksize;
	vsbp->vst_flags = sbp->st_flags;
	vsbp->vst_gen = sbp->st_gen;
	vsbp->vst_rdev = sbp->st_rdev;
	vsbp->vst_qspare[0] = sbp->st_qspare[0];
	vsbp->vst_qspare[1] = sbp->st_qspare[1];
}

int
fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, __unused boolean_t check_fsgetpath)
{

	vfs_context_t context;
	struct stat64 sb;
	int error = 0;

	bzero(&sb, sizeof(struct stat64));
	context = vfs_context_create((vfs_context_t)0);

	if (!error) {
		// FIXME:
		//error = vn_stat(vp, &sb, NULL, 1, 0, context);
		munge_vinfo_stat(&sb, &vinfo->vi_stat);
	}

	(void)vfs_context_rele(context);
	if (error != 0) {
		goto out;
	}

	if (vp->v_mount != dead_mountp) {
		vinfo->vi_fsid = vp->v_mount->mnt_vfsstat->f_fsid;
	} else {
		vinfo->vi_fsid.val[0] = 0;
		vinfo->vi_fsid.val[1] = 0;
	}
	vinfo->vi_type = vp->v_type;
out:
	return error;
}


/*
 * ======= fill_fileinfo() =======
 * From XNU: bsd/kern/proc_info.c
 * Status: Compiling
 */

// bsd/kern/kern_guarded.c
struct guarded_fileproc {
	struct fileproc gf_fileproc;
	u_int           gf_attrs;
	guardid_t       gf_guard;
};

#define FP_TO_GFP(fp)	((struct guarded_fileproc *)(fp))

/*
 * Expected use pattern:
 *
 * if (fp_isguarded(fp, GUARD_CLOSE)) {
 *      error = fp_guard_exception(p, fd, fp, kGUARD_EXC_CLOSE);
 *      proc_fdunlock(p);
 *      return error;
 * }
 *
 * Passing `0` to `attrs` returns whether the fp is guarded at all.
 */

static inline int
fp_isguarded(struct fileproc *fp, u_int attrs)
{
	if (FILEPROC_TYPE(fp) == FTYPE_GUARDED) {
		return (attrs & FP_TO_GFP(fp)->gf_attrs) == attrs;
	}
	return 0;
}

void
fill_fileinfo(struct fileproc * fp, proc_t proc, int fd, struct proc_fileinfo * fproc)
{
	fproc->fi_openflags = fp->fp_glob->fg_flag;
	fproc->fi_status = 0;
	fproc->fi_offset = fp->fp_glob->fg_offset;
	fproc->fi_type = FILEGLOB_DTYPE(fp->fp_glob);
	if (os_ref_get_count_raw(&fp->fp_glob->fg_count) > 1) {
		fproc->fi_status |= PROC_FP_SHARED;
	}
	if (proc != PROC_NULL) {
		if ((FDFLAGS_GET(proc, fd) & UF_EXCLOSE) != 0) {
			fproc->fi_status |= PROC_FP_CLEXEC;
		}
		if ((FDFLAGS_GET(proc, fd) & UF_FORKCLOSE) != 0) {
			fproc->fi_status |= PROC_FP_CLFORK;
		}
	}
	if (fp_isguarded(fp, 0)) {
		fproc->fi_status |= PROC_FP_GUARDED;
		fproc->fi_guardflags = 0;
		if (fp_isguarded(fp, GUARD_CLOSE)) {
			fproc->fi_guardflags |= PROC_FI_GUARD_CLOSE;
		}
		if (fp_isguarded(fp, GUARD_DUP)) {
			fproc->fi_guardflags |= PROC_FI_GUARD_DUP;
		}
		if (fp_isguarded(fp, GUARD_SOCKET_IPC)) {
			fproc->fi_guardflags |= PROC_FI_GUARD_SOCKET_IPC;
		}
		if (fp_isguarded(fp, GUARD_FILEPORT)) {
			fproc->fi_guardflags |= PROC_FI_GUARD_FILEPORT;
		}
	}
}


/*
 * ======= fill_socketinfo() =======
 * From XNU: bsd/kern/socket_info.c
 * Status: Not Compiling
 */
errno_t
fill_socketinfo(struct socket *so, struct socket_info *si)
{
	errno_t error = 0;
#if 0
	int domain;
	short type;
	short protocol;

	socket_lock(so, 0);

	si->soi_kind = SOCKINFO_GENERIC;

	fill_common_sockinfo(so, si);

	if (so->so_pcb == NULL || so->so_proto == 0 ||
	    so->so_proto->pr_domain == NULL) {
		goto out;
	}

	/*
	 * The kind of socket is determined by the triplet
	 * {domain, type, protocol}
	 */
	domain = SOCK_DOM(so);
	type = SOCK_TYPE(so);
	protocol = SOCK_PROTO(so);
	switch (domain) {
	case PF_INET:
	case PF_INET6: {
		struct in_sockinfo *insi = &si->soi_proto.pri_in;
		struct inpcb *inp = (struct inpcb *)so->so_pcb;

		si->soi_kind = SOCKINFO_IN;

		insi->insi_fport = inp->inp_fport;
		insi->insi_lport = inp->inp_lport;
		insi->insi_gencnt = inp->inp_gencnt;
		insi->insi_flags = inp->inp_flags;
		insi->insi_vflag = inp->inp_vflag;
		insi->insi_ip_ttl = inp->inp_ip_ttl;
		insi->insi_faddr.ina_6 = inp->inp_dependfaddr.inp6_foreign;
		insi->insi_laddr.ina_6 = inp->inp_dependladdr.inp6_local;
		insi->insi_v4.in4_tos = inp->inp_depend4.inp4_ip_tos;
		insi->insi_v6.in6_hlim = 0;
		insi->insi_v6.in6_cksum = inp->inp_depend6.inp6_cksum;
		insi->insi_v6.in6_ifindex = 0;
		insi->insi_v6.in6_hops = inp->inp_depend6.inp6_hops;

		if (type == SOCK_STREAM && (protocol == 0 ||
		    protocol == IPPROTO_TCP) && inp->inp_ppcb != NULL) {
			struct tcp_sockinfo *tcpsi = &si->soi_proto.pri_tcp;
			struct tcpcb *tp = (struct tcpcb *)inp->inp_ppcb;

			si->soi_kind = SOCKINFO_TCP;

			tcpsi->tcpsi_state = tp->t_state;
			tcpsi->tcpsi_timer[TSI_T_REXMT] =
			    tp->t_timer[TCPT_REXMT];
			tcpsi->tcpsi_timer[TSI_T_PERSIST] =
			    tp->t_timer[TCPT_PERSIST];
			tcpsi->tcpsi_timer[TSI_T_KEEP] =
			    tp->t_timer[TCPT_KEEP];
			tcpsi->tcpsi_timer[TSI_T_2MSL] =
			    tp->t_timer[TCPT_2MSL];
			tcpsi->tcpsi_mss = tp->t_maxseg;
			tcpsi->tcpsi_flags = tp->t_flags;
			tcpsi->tcpsi_tp =
			    (u_int64_t)VM_KERNEL_ADDRPERM(tp);
		}
		break;
	}
	case PF_UNIX: {
		struct unpcb *unp = (struct unpcb *)so->so_pcb;
		struct un_sockinfo *unsi = &si->soi_proto.pri_un;

		si->soi_kind = SOCKINFO_UN;

		unsi->unsi_conn_pcb =
		    (uint64_t)VM_KERNEL_ADDRPERM(unp->unp_conn);
		if (unp->unp_conn) {
			unsi->unsi_conn_so = (uint64_t)
			    VM_KERNEL_ADDRPERM(unp->unp_conn->unp_socket);
		}

		if (unp->unp_addr) {
			size_t  addrlen = unp->unp_addr->sun_len;

			if (addrlen > SOCK_MAXADDRLEN) {
				addrlen = SOCK_MAXADDRLEN;
			}
			bcopy(unp->unp_addr, &unsi->unsi_addr, addrlen);
		}
		if (unp->unp_conn && unp->unp_conn->unp_addr) {
			size_t  addrlen = unp->unp_conn->unp_addr->sun_len;

			if (addrlen > SOCK_MAXADDRLEN) {
				addrlen = SOCK_MAXADDRLEN;
			}
			bcopy(unp->unp_conn->unp_addr, &unsi->unsi_caddr,
			    addrlen);
		}
		break;
	}
	case PF_NDRV: {
		struct ndrv_cb *ndrv_cb = (struct ndrv_cb *)so->so_pcb;
		struct ndrv_info *ndrvsi = &si->soi_proto.pri_ndrv;

		si->soi_kind = SOCKINFO_NDRV;

		/* TDB lock ifnet ???? */
		if (ndrv_cb->nd_if != 0) {
			struct ifnet *ifp = ndrv_cb->nd_if;

			ndrvsi->ndrvsi_if_family = ifp->if_family;
			ndrvsi->ndrvsi_if_unit = ifp->if_unit;
			strlcpy(ndrvsi->ndrvsi_if_name, ifp->if_name, IFNAMSIZ);
		}
		break;
	}
	case PF_VSOCK: {
		const struct vsockpcb *pcb = (struct vsockpcb *)(so)->so_pcb;
		struct vsock_sockinfo *vsocksi = &si->soi_proto.pri_vsock;

		si->soi_kind = SOCKINFO_VSOCK;

		vsocksi->local_cid = pcb->local_address.cid;
		vsocksi->local_port = pcb->local_address.port;
		vsocksi->remote_cid = pcb->remote_address.cid;
		vsocksi->remote_port = pcb->remote_address.port;

		break;
	}
	case PF_SYSTEM:
		if (SOCK_PROTO(so) == SYSPROTO_EVENT) {
			struct kern_event_pcb *ev_pcb =
			    (struct kern_event_pcb *)so->so_pcb;
			struct kern_event_info *kesi =
			    &si->soi_proto.pri_kern_event;

			si->soi_kind = SOCKINFO_KERN_EVENT;

			kesi->kesi_vendor_code_filter =
			    ev_pcb->evp_vendor_code_filter;
			kesi->kesi_class_filter = ev_pcb->evp_class_filter;
			kesi->kesi_subclass_filter = ev_pcb->evp_subclass_filter;
		} else if (SOCK_PROTO(so) == SYSPROTO_CONTROL) {
			kctl_fill_socketinfo(so, si);
		}
		break;

	case PF_ROUTE:
	case PF_PPP:
	default:
		break;
	}
out:
	socket_unlock(so, 0);
#endif
	return error;
}


/*
 * ======= convert_port_to_thread() =======
 * From XNU: osfmk/kern/ipc_tt.c
 * Status: Not Compiling
 */

/*
 *	Routine:	convert_port_to_thread
 *	Purpose:
 *		Convert from a port to a thread.
 *		Doesn't consume the port ref; produces an thread ref,
 *		which may be null.
 *	Conditions:
 *		Nothing locked.
 */

static thread_t
convert_port_to_thread_locked(ipc_port_t port, port_to_thread_options_t options)
{
	thread_t thread = THREAD_NULL;
#if 0
	ip_lock_held(port);
	require_ip_active(port);

	if (ip_kotype(port) == IKOT_THREAD) {
		thread = (thread_t)port->ip_kobject;
		assert(thread != THREAD_NULL);

		if (options & PORT_TO_THREAD_NOT_CURRENT_THREAD) {
			if (thread == current_thread()) {
				return THREAD_NULL;
			}
		}

		if (options & PORT_TO_THREAD_IN_CURRENT_TASK) {
			if (thread->task != current_task()) {
				return THREAD_NULL;
			}
		} else {
			/* Use task conversion rules for thread control conversions */
			if (task_conversion_eval(current_task(), thread->task) != KERN_SUCCESS) {
				return THREAD_NULL;
			}
		}

		thread_reference_internal(thread);
	}
#endif
	return thread;
}

thread_t
convert_port_to_thread(ipc_port_t port)
{
	thread_t thread = THREAD_NULL;
#if 0
	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port)) {
			thread = convert_port_to_thread_locked(port, PORT_TO_THREAD_NONE);
		}
		ip_unlock(port);
	}
#endif
	return thread;
}
