#include <stddef.h>
#include <string.h>

#include <i386/machine_routines.h>

#include <kern/clock.h>
#include <kern/kalloc.h>
#include <kern/thread.h>

#include <mach/clock_types.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/thread_info.h>

#include <sys/cdefs.h>
#include <sys/proc_internal.h>

#include "procfs_ipc.h"
#include "procfs_locks.h"
#include "procfs_thread.h"


struct timer {
	uint64_t all_bits;
};

static inline uint64_t
timer_grab(timer_t timer)
{
	return timer->all_bits;
}

static inline uint32_t
_absolutetime_to_microtime(uint64_t abstime, clock_sec_t *secs, clock_usec_t *microsecs)
{
	uint32_t remain;
	*secs = abstime / (uint64_t)NSEC_PER_SEC;
	remain = (uint32_t)(abstime % (uint64_t)NSEC_PER_SEC);
	*microsecs = remain / NSEC_PER_USEC;
	return remain;
}

static inline void
absolutetime_to_microtime(uint64_t abstime, clock_sec_t *secs, clock_usec_t *microsecs)
{
	_absolutetime_to_microtime(abstime, secs, microsecs);
}

static inline void
thread_read_times(thread_t thread, time_value_t *user_time, time_value_t *system_time, time_value_t *runnable_time)
{
	clock_sec_t secs;
	clock_usec_t usecs;
	uint64_t  tval_user, tval_system;

	tval_user = timer_grab(&thread->user_timer);
	tval_system = timer_grab(&thread->system_timer);

	if (thread->precise_user_kernel_time) {
		absolutetime_to_microtime(tval_user, &secs, &usecs);
		user_time->seconds = (__typeof__(user_time->seconds))secs;
		user_time->microseconds = usecs;

		absolutetime_to_microtime(tval_system, &secs, &usecs);
		system_time->seconds = (__typeof__(system_time->seconds))secs;
		system_time->microseconds = usecs;
	} else {
		/* system_timer may represent either sys or user */
		tval_user += tval_system;
		absolutetime_to_microtime(tval_user, &secs, &usecs);
		user_time->seconds = (__typeof__(user_time->seconds))secs;
		user_time->microseconds = usecs;

		system_time->seconds = 0;
		system_time->microseconds = 0;
	}

	if (runnable_time) {
		uint64_t tval_runnable = timer_grab(&thread->runnable_timer);
		absolutetime_to_microtime(tval_runnable, &secs, &usecs);
		runnable_time->seconds = (__typeof__(runnable_time->seconds))secs;
		runnable_time->microseconds = usecs;
	}
}

static inline void
retrieve_thread_basic_info(thread_t thread, thread_basic_info_t basic_info)
{
	int     state, flags;

	/* fill in info */

	thread_read_times(thread, &basic_info->user_time,
	    &basic_info->system_time, NULL);

	/*
	 *	Update lazy-evaluated scheduler info because someone wants it.
	 */
	if (SCHED(can_update_priority)(thread)) {
		SCHED(update_priority)(thread);
	}

	basic_info->sleep_time = 0;

	/*
	 *	To calculate cpu_usage, first correct for timer rate,
	 *	then for 5/8 ageing.  The correction factor [3/5] is
	 *	(1/(5/8) - 1).
	 */
	basic_info->cpu_usage = 0;

	if (basic_info->cpu_usage > TH_USAGE_SCALE) {
		basic_info->cpu_usage = TH_USAGE_SCALE;
	}

	basic_info->policy = ((thread->sched_mode == TH_MODE_TIMESHARE)?
	    POLICY_TIMESHARE: POLICY_RR);

	flags = 0;
	if (thread->options & TH_OPT_IDLE_THREAD) {
		flags |= TH_FLAGS_IDLE;
	}

	if (thread->options & TH_OPT_GLOBAL_FORCED_IDLE) {
		flags |= TH_FLAGS_GLOBAL_FORCED_IDLE;
	}

	if (!thread->kernel_stack) {
		flags |= TH_FLAGS_SWAPPED;
	}

	state = 0;
	if (thread->state & TH_TERMINATE) {
		state = TH_STATE_HALTED;
	} else if (thread->state & TH_RUN) {
		state = TH_STATE_RUNNING;
	} else if (thread->state & TH_UNINT) {
		state = TH_STATE_UNINTERRUPTIBLE;
	} else if (thread->state & TH_SUSP) {
		state = TH_STATE_STOPPED;
	} else if (thread->state & TH_WAIT) {
		state = TH_STATE_WAITING;
	}

	basic_info->run_state = state;
	basic_info->flags = flags;

	basic_info->suspend_count = thread->user_stop_count;

	return;
}

static inline uint64_t
get_task_dispatchqueue_offset(task_t task)
{
	return task->dispatchqueue_offset;
}

static inline uint64_t
get_dispatchqueue_offset_from_proc(void *p)
{
	if (p != NULL) {
		proc_t pself = (proc_t)p;
		return pself->p_dispatchqueue_offset;
	} else {
		return (uint64_t)0;
	}
}

static inline uint64_t
thread_dispatchqaddr(thread_t thread)
{
	uint64_t        dispatchqueue_addr;
	uint64_t        thread_handle;

	if (thread == THREAD_NULL) {
		return 0;
	}

	thread_handle = thread->machine.cthread_self;
	if (thread_handle == 0) {
		return 0;
	}

	if (thread->inspection == TRUE) {
		dispatchqueue_addr = thread_handle + get_task_dispatchqueue_offset(thread->task);
	} else if (thread->task->bsd_info) {
		dispatchqueue_addr = thread_handle + get_dispatchqueue_offset_from_proc(thread->task->bsd_info);
	} else {
		dispatchqueue_addr = 0;
	}

	return dispatchqueue_addr;
}

kern_return_t
thread_info_internal(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info_out, mach_msg_type_number_t *thread_info_count)
{
	spl_t s;
	s = splsched();

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (flavor == THREAD_BASIC_INFO) {
		if (*thread_info_count < THREAD_BASIC_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		thread_lock(thread);

		retrieve_thread_basic_info(thread, (thread_basic_info_t) thread_info_out);

		thread_unlock(thread);
		splx(s);

		*thread_info_count = THREAD_BASIC_INFO_COUNT;

		return KERN_SUCCESS;

	} else if (flavor == THREAD_IDENTIFIER_INFO) {
		thread_identifier_info_t identifier_info;
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
		//FIXME:
		extended_info->pth_curpri = thread->sched_pri;
		extended_info->pth_priority = thread->base_pri;
		extended_info->pth_maxpriority = thread->max_priority;

		//FIXME:
		//bsd_getthreadname(thread->uthread, extended_info->pth_name);

		thread_unlock(thread);
		splx(s);

		*thread_info_count = THREAD_EXTENDED_INFO_COUNT;

		return KERN_SUCCESS;
	} else if (flavor == THREAD_DEBUG_INFO_INTERNAL) {
		return KERN_NOT_SUPPORTED;
	}
	return KERN_INVALID_ARGUMENT;
}

kern_return_t
thread_info(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count)
{
	kern_return_t result;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_lock(thread);

	thread = current_thread();

	if (thread != THREAD_NULL) {
		result = thread_info_internal(
			thread, flavor, thread_info, thread_info_count);
	} else {
		result = KERN_TERMINATED;
	}

	thread_unlock(thread);

	return result;
}

static inline kern_return_t
task_threads_internal(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count, mach_thread_flavor_t flavor)
{
	mach_msg_type_number_t  actual;
	thread_t *thread_list;
	thread_t thread;
	vm_size_t size, size_needed;
	void *addr;
	unsigned int i, j;
	size = 0;
	addr = NULL;

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
	return KERN_SUCCESS;
}

kern_return_t
task_threads(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count)
{
	return task_threads_internal(task, threads_out, count, THREAD_FLAVOR_CONTROL);
}
