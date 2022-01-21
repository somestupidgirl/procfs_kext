#include <stdatomic.h>

#include <mach/message.h>
#include <mach/task.h>
#include <mach/thread_info.h>
#include <mach/i386/kern_return.h>

#include <os/refcnt.h>

#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/guarded.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/types.h>
#include <sys/errno.h>

#include "procfs_kernel.h"
#include "procfs_locks.h"

#if 0
/*
 * ======= thread_info() =======
 * From XNU: osfmk/kern/thread_act.c
 */

kern_return_t
thread_info(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info_out, mach_msg_type_number_t *thread_info_count)
{
	kern_return_t                   result;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active || thread->inspection) {
		result = thread_info_internal(
			thread, flavor, thread_info_out, thread_info_count);
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(thread);

	return result;
}

/*
 * ======= convert_port_to_thread() =======
 * From XNU: osfmk/kern/ipc_tt.c
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
convert_port_to_thread_locked(
	ipc_port_t               port,
	port_to_thread_options_t options)
{
	thread_t        thread = THREAD_NULL;

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

	return thread;
}

thread_t
convert_port_to_thread(
	ipc_port_t              port)
{
	thread_t        thread = THREAD_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port)) {
			thread = convert_port_to_thread_locked(port, PORT_TO_THREAD_NONE);
		}
		ip_unlock(port);
	}

	return thread;
}


/*
 * ======= fp_getfvpandvid() =======
 * From XNU: bsd/kern/kern_descrip.c
 *
 * - Deprecated Function
 */

/*
 * fp_getfvpandvid
 *
 * Description:	Get fileproc, vnode pointer, and vid for a given fd from the
 *		per process open file table of the specified process, and if
 *		successful, increment the f_iocount
 *
 * Parameters:	p				Process in which fd lives
 *		fd				fd to get information for
 *		resultfp			Pointer to result fileproc
 *						pointer area, or 0 if none
 *		resultvp			Pointer to result vnode pointer
 *						area, or 0 if none
 *		vidp				Pointer to resuld vid area
 *
 * Returns:	0				Success
 *		EBADF				Bad file descriptor
 *		ENOTSUP				fd does not refer to a vnode
 *
 * Implicit returns:
 *		*resultfp (modified)		Fileproc pointer
 *		*resultvp (modified)		vnode pointer
 *		*vidp				vid value
 *
 * Notes:	The resultfp and resultvp fields are optional, and may be
 *		independently specified as NULL to skip returning information
 *
 * Locks:	Internally takes and releases proc_fdlock
 */
int
fp_getfvpandvid(proc_t p, int fd, struct fileproc **resultfp,
		struct vnode **resultvp, uint32_t *vidp)
{
	struct filedesc *fdp = p->p_fd;
	struct fileproc *fp;

	proc_fdlock_spin(p);
	if (fd < 0 || fd >= fdp->fd_nfiles ||
			(fp = fdp->fd_ofiles[fd]) == NULL ||
			(fdp->fd_ofileflags[fd] & UF_RESERVED)) {
		proc_fdunlock(p);
		return (EBADF);
	}
	if (fp->f_type != DTYPE_VNODE) {
		proc_fdunlock(p);
		return(ENOTSUP);
	}
	fp->f_iocount++;

	if (resultfp)
		*resultfp = fp;
	if (resultvp)
		*resultvp = (struct vnode *)fp->f_data;
	if (vidp)
		*vidp = (uint32_t)vnode_vid((struct vnode *)fp->f_data);
	proc_fdunlock(p);

	return (0);
}


/*
 * ======= fp_getfsock() =======
 * From XNU: bsd/kern/kern_descrip.c
 *
 * - Deprecated Function
 */

/*
 * fp_getfsock
 *
 * Description:	Get fileproc and socket pointer for a given fd from the
 *		per process open file table of the specified process, and if
 *		successful, increment the f_iocount
 *
 * Parameters:	p				Process in which fd lives
 *		fd				fd to get information for
 *		resultfp			Pointer to result fileproc
 *						pointer area, or 0 if none
 *		results				Pointer to result socket
 *						pointer area, or 0 if none
 *
 * Returns:	EBADF			The file descriptor is invalid
 *		EOPNOTSUPP		The file descriptor is not a socket
 *		0			Success
 *
 * Implicit returns:
 *		*resultfp (modified)		Fileproc pointer
 *		*results (modified)		socket pointer
 *
 * Notes:	EOPNOTSUPP should probably be ENOTSOCK; this function is only
 *		ever called from accept1().
 */

int
fp_getfsock(proc_t p, int fd, struct fileproc **resultfp,
	    struct socket **results)
{
	struct filedesc *fdp = p->p_fd;
	struct fileproc *fp;

	proc_fdlock_spin(p);
	if (fd < 0 || fd >= fdp->fd_nfiles ||
			(fp = fdp->fd_ofiles[fd]) == NULL ||
			(fdp->fd_ofileflags[fd] & UF_RESERVED)) {
		proc_fdunlock(p);
		return (EBADF);
	}
	if (fp->f_type != DTYPE_SOCKET) {
		proc_fdunlock(p);
		return(EOPNOTSUPP);
	}
	fp->f_iocount++;

	if (resultfp)
		*resultfp = fp;
	if (results)
		*results = (struct socket *)fp->f_data;
	proc_fdunlock(p);

	return (0);
}
#endif


/*
 * ======= fill_fileinfo() =======
 * From XNU: bsd/kern/proc_info.c
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