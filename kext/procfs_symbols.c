#include <mach/kern_return.h>

#include "procfs_symbols.h"

// Global Variables
SYM_INIT(nprocs);
SYM_INIT(maxproc);
SYM_INIT(maxprocperuid);
SYM_INIT(hard_maxproc);

// Process Locks
SYM_INIT(proc_lock);
SYM_INIT(proc_unlock);
SYM_INIT(proc_spinlock);
SYM_INIT(proc_spinunlock);
SYM_INIT(proc_list_lock);
SYM_INIT(proc_list_unlock);
SYM_INIT(proc_fdlock);
SYM_INIT(proc_fdlock_spin);
SYM_INIT(proc_fdunlock);
SYM_INIT(proc_fdlock_assert);

// Iteration
SYM_INIT(proc_iterate);
SYM_INIT(pgrp_iterate);

// Process Group and Session
SYM_INIT(proc_pgrp);
SYM_INIT(tty_pgrp);
SYM_INIT(proc_session);

// Process Name
SYM_INIT(proc_best_name);

// Process TTY
SYM_INIT(proc_gettty);
SYM_INIT(proc_gettty_dev);

// Process UID/GID
SYM_INIT(proc_getuid);
SYM_INIT(proc_getgid);

// Process Task
SYM_INIT(proc_task);

// File Descriptor Table
SYM_INIT(fdt_next);

// Thread
SYM_INIT(convert_port_to_thread);
SYM_INIT(task_threads);
SYM_INIT(thread_info);

kern_return_t
resolve_symbols()
{
	// Global Variables
	SYM_LOOKUP(nprocs);
	SYM_LOOKUP(maxproc);
	SYM_LOOKUP(maxprocperuid);
	SYM_LOOKUP(hard_maxproc);

	// Process Locks
	SYM_LOOKUP(proc_lock);
	SYM_LOOKUP(proc_unlock);
	SYM_LOOKUP(proc_spinlock);
	SYM_LOOKUP(proc_spinunlock);
	SYM_LOOKUP(proc_list_lock);
	SYM_LOOKUP(proc_list_unlock);
	SYM_LOOKUP(proc_fdlock);
	SYM_LOOKUP(proc_fdlock_spin);
	SYM_LOOKUP(proc_fdunlock);
	SYM_LOOKUP(proc_fdlock_assert);

	// Process Iteration
	SYM_LOOKUP(proc_iterate);
	SYM_LOOKUP(pgrp_iterate);

	// Process Group and Session
	SYM_LOOKUP(proc_pgrp);
	SYM_LOOKUP(tty_pgrp);
	SYM_LOOKUP(proc_session);

	// Process Name
	SYM_LOOKUP(proc_best_name);

	// Process TTY
	SYM_LOOKUP(proc_gettty);
	SYM_LOOKUP(proc_gettty_dev);

	// Process UID/GID
	SYM_LOOKUP(proc_getuid);
	SYM_LOOKUP(proc_getgid);

	// Process Task
	SYM_LOOKUP(proc_task);

	// File Descriptor
	SYM_LOOKUP(fdt_next);

	// Thread
	SYM_LOOKUP(convert_port_to_thread);
	SYM_LOOKUP(task_threads);
	SYM_LOOKUP(thread_info);
				   
	return KERN_SUCCESS;
}
