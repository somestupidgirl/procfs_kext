#include <stddef.h>
#include <string.h>

#include <i386/machine_routines.h>

#include <kern/clock.h>
#include <kern/kalloc.h>
#include <kern/thread.h>

#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/thread_info.h>

#include <sys/cdefs.h>

#include "procfs_ipc.h"
#include "procfs_locks.h"
#include "procfs_thread.h"


static inline kern_return_t
procfs_thread_info_internal(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info_out, mach_msg_type_number_t *thread_info_count)
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

		//FIXME:
		//retrieve_thread_basic_info(thread, (thread_basic_info_t) thread_info_out);

		thread_unlock(thread);
		splx(s);

		*thread_info_count = THREAD_BASIC_INFO_COUNT;

		return KERN_SUCCESS;

	} else if (flavor == THREAD_IDENTIFIER_INFO) {
		thread_identifier_info_t identifier_info;
		thread_lock(thread);

		identifier_info->thread_id = thread->thread_id;
		identifier_info->thread_handle = thread->machine.cthread_self;
		//FIXME:
		//identifier_info->dispatch_qaddr = thread_dispatchqaddr(thread);

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
		//FIXME:
		//retrieve_thread_basic_info(thread, &basic_info);
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
procfs_thread_info(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count)
{
	kern_return_t result;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_lock(thread);

	thread = current_thread();

	if (thread != THREAD_NULL) {
		result = procfs_thread_info_internal(
			thread, flavor, thread_info, thread_info_count);
	} else {
		result = KERN_TERMINATED;
	}

	thread_unlock(thread);

	return result;
}

static inline kern_return_t
procfs_task_threads_internal(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count, mach_thread_flavor_t flavor)
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
		//FIXME:
		//thread_reference_internal(thread);
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
procfs_task_threads(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count)
{
	return procfs_task_threads_internal(task, threads_out, count, THREAD_FLAVOR_CONTROL);
}
