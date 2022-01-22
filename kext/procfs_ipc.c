#include <kern/debug.h>
#include <kern/locks.h>
#include <kern/task.h>
#include <kern/thread.h>

#include <mach/mach_types.h>
#include <mach/port.h>

#include <sys/cdefs.h>

#include "procfs_ipc.h"
#include "procfs_locks.h"
#include "procfs_thread.h"

void
io_lock(ipc_object_t io)
{
	//FIXME:
	//ipc_object_validate(io);
	//lck_spin_lock_grp(&(io)->io_lock_data, &ipc_lck_grp);
}

boolean_t
io_lock_try(ipc_object_t io)
{
	//FIXME:
	//ipc_object_validate(io);
	//return lck_spin_try_lock_grp(&(io)->io_lock_data, &ipc_lck_grp);
}

static inline void
require_ip_active(ipc_port_t port)
{
	if (!ip_active(port)) {
		panic("Using inactive port %p", port);
	}
}

static inline void
task_require(struct task *task)
{
	//FIXME:
	//zone_id_require(ZONE_ID_TASK, sizeof(struct task), task);
}

static inline kern_return_t
task_conversion_eval(task_t caller, task_t victim)
{
	/*
	 * Tasks are allowed to resolve their own task ports, and the kernel is
	 * allowed to resolve anyone's task port.
	 */
	if (caller == kernel_task) {
		return KERN_SUCCESS;
	}

	if (caller == victim) {
		return KERN_SUCCESS;
	}

	/*
	 * Only the kernel can can resolve the kernel's task port. We've established
	 * by this point that the caller is not kernel_task.
	 */
	if (victim == TASK_NULL || victim == kernel_task) {
		return KERN_INVALID_SECURITY;
	}

	task_require(victim);

	return KERN_SUCCESS;
}

static inline thread_t
convert_port_to_thread_locked(ipc_port_t port, port_to_thread_options_t options, boolean_t eval)
{
	thread_t thread = THREAD_NULL;

	ip_lock_held(port);
	require_ip_active(port);

	if (ip_kotype(port) == IKOT_THREAD_CONTROL) {
		thread = (thread_t) ip_get_kobject(port);
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
			if (eval && task_conversion_eval(current_task(), thread->task) != KERN_SUCCESS) {
				return THREAD_NULL;
			}
		}

		thread_reference_internal(thread);
	}

	return thread;
}

thread_t
convert_port_to_thread(ipc_port_t port)
{
	thread_t thread = THREAD_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port)) {
			thread = convert_port_to_thread_locked(port, PORT_TO_THREAD_NONE, TRUE);
		}
		ip_unlock(port);
	}

	return thread;
}

static inline ipc_port_t
ipc_port_make_send_locked(ipc_port_t port)
{
	require_ip_active(port);
	port->ip_mscount++;
	port->ip_srights++;
	ip_reference(port);
	return port;
}

static inline ipc_port_t
ipc_port_make_send(ipc_port_t port)
{
	if (!IP_VALID(port)) {
		return port;
	}

	ip_lock(port);
	if (ip_active(port)) {
		ipc_port_make_send_locked(port);
		ip_unlock(port);
		return port;
	}
	ip_unlock(port);
	return IP_DEAD;
}

static inline ipc_port_t
convert_thread_to_port_with_flavor(thread_t thread, mach_thread_flavor_t flavor)
{
	ipc_port_t port = IP_NULL;

	thread_lock(thread);

	if (!thread->ipc_active) {
		goto exit;
	}

	if (flavor == THREAD_FLAVOR_CONTROL) {
		port = ipc_port_make_send(thread->ith_thread_ports[flavor]);
	} else {
		ipc_kobject_type_t kotype = (flavor == THREAD_FLAVOR_READ) ? IKOT_THREAD_READ : IKOT_THREAD_INSPECT;
		/*
		 * Claim a send right on the thread read/inspect port, and request a no-senders
		 * notification on that port (if none outstanding). A thread reference is not
		 * donated here even though the ports are created lazily because it doesn't own the
		 * kobject that it points to. Threads manage their lifetime explicitly and
		 * have to synchronize with each other, between the task/thread terminating and the
		 * send-once notification firing, and this is done under the thread mutex
		 * rather than with atomics.
		 */
		//FIXME:
		//(void)ipc_kobject_make_send_lazy_alloc_port(&thread->ith_thread_ports[flavor], (ipc_kobject_t)thread,
		//    kotype, IPC_KOBJECT_ALLOC_IMMOVABLE_SEND, false, 0);
		port = thread->ith_thread_ports[flavor];
	}

exit:
	thread_unlock(thread);
	thread_deallocate(thread);
	return port;
}

ipc_port_t
convert_thread_to_port(thread_t thread)
{
	return convert_thread_to_port_with_flavor(thread, THREAD_FLAVOR_CONTROL);
}

ipc_port_t
convert_thread_read_to_port(thread_read_t thread)
{
	return convert_thread_to_port_with_flavor(thread, THREAD_FLAVOR_READ);
}

ipc_port_t
convert_thread_inspect_to_port(thread_inspect_t thread)
{
	return convert_thread_to_port_with_flavor(thread, THREAD_FLAVOR_INSPECT);
}
