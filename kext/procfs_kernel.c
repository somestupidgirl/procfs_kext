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
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/types.h>
#include <sys/vnode.h>

#include "procfs_kernel.h"
#include "procfs_locks.h"


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
 * ======= fp_drop() =======
 * From XNU: bsd/kern/kern_descrip.c
 * Status: Compiling
 */

/*
 * fp_drop
 *
 * Description:	Drop the I/O reference previously taken by calling fp_lookup
 *		et. al.
 *
 * Parameters:	p				Process in which the fd lives
 *		fd				fd associated with the fileproc
 *		fp				fileproc on which to set the
 *						flag and drop the reference
 *		locked				flag to internally take and
 *						drop proc_fdlock if it is not
 *						already held by the caller
 *
 * Returns:	0				Success
 *		EBADF				Bad file descriptor
 *
 * Locks:	This function internally takes and drops the proc_fdlock for
 *		the supplied process if 'locked' is non-zero, and assumes that
 *		the caller already holds this lock if 'locked' is non-zero.
 *
 * Notes:	The fileproc must correspond to the fd in the supplied proc
 */
int
fp_drop(proc_t p, int fd, struct fileproc *fp, int locked)
{
	struct filedesc *fdp = p->p_fd;
	int     needwakeup = 0;

	if (!locked) {
		procfs_fdlock_spin(p);
	}
	if ((fp == FILEPROC_NULL) && (fd < 0 || fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL ||
	    ((fdp->fd_ofileflags[fd] & UF_RESERVED) &&
	    !(fdp->fd_ofileflags[fd] & UF_CLOSING)))) {
		if (!locked) {
			procfs_fdunlock(p);
		}
		return EBADF;
	}

	if (1 == os_ref_release_locked(&fp->fp_iocount)) {
		if (fp->fp_flags & FP_SELCONFLICT) {
			fp->fp_flags &= ~FP_SELCONFLICT;
		}

		if (p->p_fpdrainwait) {
			p->p_fpdrainwait = 0;
			needwakeup = 1;
		}
	}
	if (!locked) {
		procfs_fdunlock(p);
	}
	if (needwakeup) {
		wakeup(&p->p_fpdrainwait);
	}

	return 0;
}


/*
 * ======= fp_getfvpandvid() =======
 * From XNU: bsd/kern/kern_descrip.c
 * Status: Not Compiling
 * Note: Deprecated Function
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
#if 0
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
#endif
	return (0);
}

/*
 * ======= fp_getfsock() =======
 * From XNU: bsd/kern/kern_descrip.c
 * Status: Not Compiling
 * Note: Deprecated Function
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
fp_getfsock(proc_t p, int fd, struct fileproc **resultfp, struct socket **results)
{
#if 0
	struct filedesc *fdp = p->p_fd;
	struct fileproc *fp;

	procfs_fdlock_spin(p);
	if (fd < 0 || fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL ||
	    (fdp->fd_ofileflags[fd] & UF_RESERVED)) {
		procfs_fdunlock(p);
		return EBADF;
	}
	if (fp->fp_type != DTYPE_SOCKET) {
		procfs_fdunlock(p);
		return EOPNOTSUPP;
	}
	os_ref_retain_locked(&fp->fp_iocount);

	if (resultfp) {
		*resultfp = fp;
	}
	if (results) {
		*results = (struct socket *)fp->fp_data;
	}
	procfs_fdunlock(p);
#endif
	return 0;
}


/*
 * ======= thread_info() =======
 * From XNU: osfmk/kern/thread.c
 * Status: Not Compiling
 */

static inline kern_return_t
thread_info_internal(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info_out, mach_msg_type_number_t *thread_info_count)
{
#if 0
	spl_t   s;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (flavor == THREAD_BASIC_INFO) {
		if (*thread_info_count < THREAD_BASIC_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		s = splsched();
		thread_lock(thread);

		retrieve_thread_basic_info(thread, (thread_basic_info_t) thread_info_out);

		thread_unlock(thread);
		splx(s);

		*thread_info_count = THREAD_BASIC_INFO_COUNT;

		return KERN_SUCCESS;
	} else if (flavor == THREAD_IDENTIFIER_INFO) {
		thread_identifier_info_t        identifier_info;

		if (*thread_info_count < THREAD_IDENTIFIER_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		identifier_info = __IGNORE_WCASTALIGN((thread_identifier_info_t)thread_info_out);

		s = splsched();
		thread_lock(thread);

		identifier_info->thread_id = thread->thread_id;
		identifier_info->thread_handle = thread->machine.cthread_self;
		identifier_info->dispatch_qaddr = thread_dispatchqaddr(thread);

		thread_unlock(thread);
		splx(s);
		return KERN_SUCCESS;
	} else if (flavor == THREAD_SCHED_TIMESHARE_INFO) {
		policy_timeshare_info_t         ts_info;

		if (*thread_info_count < POLICY_TIMESHARE_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		ts_info = (policy_timeshare_info_t)thread_info_out;

		s = splsched();
		thread_lock(thread);

		if (thread->sched_mode != TH_MODE_TIMESHARE) {
			thread_unlock(thread);
			splx(s);
			return KERN_INVALID_POLICY;
		}

		ts_info->depressed = (thread->sched_flags & TH_SFLAG_DEPRESSED_MASK) != 0;
		if (ts_info->depressed) {
			ts_info->base_priority = DEPRESSPRI;
			ts_info->depress_priority = thread->base_pri;
		} else {
			ts_info->base_priority = thread->base_pri;
			ts_info->depress_priority = -1;
		}

		ts_info->cur_priority = thread->sched_pri;
		ts_info->max_priority = thread->max_priority;

		thread_unlock(thread);
		splx(s);

		*thread_info_count = POLICY_TIMESHARE_INFO_COUNT;

		return KERN_SUCCESS;
	} else if (flavor == THREAD_SCHED_FIFO_INFO) {
		if (*thread_info_count < POLICY_FIFO_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		return KERN_INVALID_POLICY;
	} else if (flavor == THREAD_SCHED_RR_INFO) {
		policy_rr_info_t                        rr_info;
		uint32_t quantum_time;
		uint64_t quantum_ns;

		if (*thread_info_count < POLICY_RR_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		rr_info = (policy_rr_info_t) thread_info_out;

		s = splsched();
		thread_lock(thread);

		if (thread->sched_mode == TH_MODE_TIMESHARE) {
			thread_unlock(thread);
			splx(s);

			return KERN_INVALID_POLICY;
		}

		rr_info->depressed = (thread->sched_flags & TH_SFLAG_DEPRESSED_MASK) != 0;
		if (rr_info->depressed) {
			rr_info->base_priority = DEPRESSPRI;
			rr_info->depress_priority = thread->base_pri;
		} else {
			rr_info->base_priority = thread->base_pri;
			rr_info->depress_priority = -1;
		}

		quantum_time = SCHED(initial_quantum_size)(THREAD_NULL);
		absolutetime_to_nanoseconds(quantum_time, &quantum_ns);

		rr_info->max_priority = thread->max_priority;
		rr_info->quantum = (uint32_t)(quantum_ns / 1000 / 1000);

		thread_unlock(thread);
		splx(s);

		*thread_info_count = POLICY_RR_INFO_COUNT;

		return KERN_SUCCESS;
	} else if (flavor == THREAD_EXTENDED_INFO) {
		thread_basic_info_data_t        basic_info;
		thread_extended_info_t          extended_info = __IGNORE_WCASTALIGN((thread_extended_info_t)thread_info_out);

		if (*thread_info_count < THREAD_EXTENDED_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		s = splsched();
		thread_lock(thread);

		/* NOTE: This mimics fill_taskthreadinfo(), which is the function used by proc_pidinfo() for
		 * the PROC_PIDTHREADINFO flavor (which can't be used on corpses)
		 */
		retrieve_thread_basic_info(thread, &basic_info);
		extended_info->pth_user_time = (((uint64_t)basic_info.user_time.seconds * NSEC_PER_SEC) + ((uint64_t)basic_info.user_time.microseconds * NSEC_PER_USEC));
		extended_info->pth_system_time = (((uint64_t)basic_info.system_time.seconds * NSEC_PER_SEC) + ((uint64_t)basic_info.system_time.microseconds * NSEC_PER_USEC));

		extended_info->pth_cpu_usage = basic_info.cpu_usage;
		extended_info->pth_policy = basic_info.policy;
		extended_info->pth_run_state = basic_info.run_state;
		extended_info->pth_flags = basic_info.flags;
		extended_info->pth_sleep_time = basic_info.sleep_time;
		extended_info->pth_curpri = thread->sched_pri;
		extended_info->pth_priority = thread->base_pri;
		extended_info->pth_maxpriority = thread->max_priority;

		bsd_getthreadname(thread->uthread, extended_info->pth_name);

		thread_unlock(thread);
		splx(s);

		*thread_info_count = THREAD_EXTENDED_INFO_COUNT;

		return KERN_SUCCESS;
	} else if (flavor == THREAD_DEBUG_INFO_INTERNAL) {
#if DEVELOPMENT || DEBUG
		thread_debug_info_internal_t dbg_info;
		if (*thread_info_count < THREAD_DEBUG_INFO_INTERNAL_COUNT) {
			return KERN_NOT_SUPPORTED;
		}

		if (thread_info_out == NULL) {
			return KERN_INVALID_ARGUMENT;
		}

		dbg_info = __IGNORE_WCASTALIGN((thread_debug_info_internal_t)thread_info_out);
		dbg_info->page_creation_count = thread->t_page_creation_count;

		*thread_info_count = THREAD_DEBUG_INFO_INTERNAL_COUNT;
		return KERN_SUCCESS;
#endif /* DEVELOPMENT || DEBUG */
		return KERN_NOT_SUPPORTED;
	}
#endif
	return KERN_INVALID_ARGUMENT;
}


/*
 * ======= thread_info() =======
 * From XNU: osfmk/kern/thread_act.c
 * Status: Not Compiling
 */

kern_return_t
thread_info(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count)
{
	kern_return_t result;
#if 0
	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active || thread->inspection) {
		result = thread_info_internal(
			thread, flavor, thread_info, thread_info_count);
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(thread);
#endif
	return result;
}


/*
 * ======= task_threads() =======
 * From XNU: osfmk/kern/task.c
 */

static inline kern_return_t
task_threads_internal(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count, mach_thread_flavor_t flavor)
{
#if 0
	mach_msg_type_number_t  actual;
	thread_t *thread_list;
	thread_t thread;
	vm_size_t size, size_needed;
	void *addr;
	unsigned int i, j;

	size = 0; addr = NULL;

	if (task == TASK_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	for (;;) {
		task_lock(task);
		if (!task->active) {
			task_unlock(task);

			if (size != 0) {
				kfree(addr, size);
			}

			return KERN_FAILURE;
		}

		actual = task->thread_count;

		/* do we have the memory we need? */
		size_needed = actual * sizeof(mach_port_t);
		if (size_needed <= size) {
			break;
		}

		/* unlock the task and allocate more memory */
		task_unlock(task);

		if (size != 0) {
			kfree(addr, size);
		}

		assert(size_needed > 0);
		size = size_needed;

		addr = kalloc(size);
		if (addr == 0) {
			return KERN_RESOURCE_SHORTAGE;
		}
	}

	/* OK, have memory and the task is locked & active */
	thread_list = (thread_t *)addr;

	i = j = 0;

	for (thread = (thread_t)queue_first(&task->threads); i < actual;
	    ++i, thread = (thread_t)queue_next(&thread->task_threads)) {
		thread_reference_internal(thread);
		thread_list[j++] = thread;
	}

	assert(queue_end(&task->threads, (queue_entry_t)thread));

	actual = j;
	size_needed = actual * sizeof(mach_port_t);

	/* can unlock task now that we've got the thread refs */
	task_unlock(task);

	if (actual == 0) {
		/* no threads, so return null pointer and deallocate memory */

		*threads_out = NULL;
		*count = 0;

		if (size != 0) {
			kfree(addr, size);
		}
	} else {
		/* if we allocated too much, must copy */

		if (size_needed < size) {
			void *newaddr;

			newaddr = kalloc(size_needed);
			if (newaddr == 0) {
				for (i = 0; i < actual; ++i) {
					thread_deallocate(thread_list[i]);
				}
				kfree(addr, size);
				return KERN_RESOURCE_SHORTAGE;
			}

			bcopy(addr, newaddr, size_needed);
			kfree(addr, size);
			thread_list = (thread_t *)newaddr;
		}

		*threads_out = thread_list;
		*count = actual;

		/* do the conversion that Mig should handle */

		switch (flavor) {
		case THREAD_FLAVOR_CONTROL:
			for (i = 0; i < actual; ++i) {
				((ipc_port_t *) thread_list)[i] = convert_thread_to_port(thread_list[i]);
			}
			break;
		case THREAD_FLAVOR_READ:
			for (i = 0; i < actual; ++i) {
				((ipc_port_t *) thread_list)[i] = convert_thread_read_to_port(thread_list[i]);
			}
			break;
		case THREAD_FLAVOR_INSPECT:
			for (i = 0; i < actual; ++i) {
				((ipc_port_t *) thread_list)[i] = convert_thread_inspect_to_port(thread_list[i]);
			}
			break;
		default:
			return KERN_INVALID_ARGUMENT;
		}
	}
#endif
	return KERN_SUCCESS;
}

kern_return_t
task_threads(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count)
{
	return task_threads_internal(task, threads_out, count, THREAD_FLAVOR_CONTROL);
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
