#include <mach/kern_return.h>

#include "symbols.h"

// Global Variables
SYM_INIT(initproc);
SYM_INIT(allproc);
SYM_INIT(nprocs);
SYM_INIT(maxproc);
SYM_INIT(maxprocperuid);
SYM_INIT(hard_maxproc);
SYM_INIT(processor_count);
SYM_INIT(tscFreq);

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

// UID/GID
SYM_INIT(proc_issetugid);

// Process Name
SYM_INIT(proc_name);

// Process TTY
SYM_INIT(proc_gettty);
SYM_INIT(proc_gettty_dev);

// Process Start Time
SYM_INIT(proc_starttime);

// Process Task
SYM_INIT(proc_task);

// Process Info
SYM_INIT(fill_socketinfo);
SYM_INIT(fill_taskprocinfo);
SYM_INIT(fill_taskthreadinfo);

// Process Misc
SYM_INIT(proc_get_darwinbgstate);
SYM_INIT(proc_reparentlocked);

// File Descriptor
SYM_INIT(proc_fdlist);
SYM_INIT(fdt_next);

// Thread
SYM_INIT(convert_port_to_thread);
SYM_INIT(task_threads);
SYM_INIT(thread_info);

// CPU
SYM_INIT(cpuid_info);
SYM_INIT(cpu_number);
SYM_INIT(cpuid_features);
SYM_INIT(cpuid_extfeatures);
SYM_INIT(cpuid_leaf7_features);
SYM_INIT(cpuid_leaf7_extfeatures);
SYM_INIT(cpuid_get_feature_names);
SYM_INIT(cpuid_get_extfeature_names);
SYM_INIT(cpuid_get_leaf7_feature_names);
SYM_INIT(cpuid_get_leaf7_extfeature_names);

// Vnode
SYM_INIT(vn_stat);

kern_return_t
resolve_symbols()
{
	// Global Variables
	SYM_LOOKUP(initproc);
	SYM_LOOKUP(allproc);
	SYM_LOOKUP(nprocs);
	SYM_LOOKUP(maxproc);
	SYM_LOOKUP(maxprocperuid);
	SYM_LOOKUP(hard_maxproc);
	SYM_LOOKUP(processor_count);
	SYM_LOOKUP(tscFreq);

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

	// UID/GID
	SYM_LOOKUP(proc_issetugid);

	// Process Name
	SYM_LOOKUP(proc_name);

	// Process TTY
	SYM_LOOKUP(proc_gettty);
	SYM_LOOKUP(proc_gettty_dev);

	// Process Start Time
	SYM_LOOKUP(proc_starttime);

	// Process Task
	SYM_LOOKUP(proc_task);

	// Process Info
	SYM_LOOKUP(fill_socketinfo);
	SYM_LOOKUP(fill_taskprocinfo);
	SYM_LOOKUP(fill_taskthreadinfo);

	// Process Misc
	SYM_LOOKUP(proc_get_darwinbgstate);
	SYM_LOOKUP(proc_reparentlocked);

	// File Descriptor
	SYM_LOOKUP(proc_fdlist);
	SYM_LOOKUP(fdt_next);

	// Thread
	SYM_LOOKUP(convert_port_to_thread);
	SYM_LOOKUP(task_threads);
	SYM_LOOKUP(thread_info);

	// CPU ID
	SYM_LOOKUP(cpu_number);
	SYM_LOOKUP(cpuid_info);
	SYM_LOOKUP(cpuid_features);
	SYM_LOOKUP(cpuid_extfeatures);
	SYM_LOOKUP(cpuid_leaf7_features);
	SYM_LOOKUP(cpuid_leaf7_extfeatures);
	SYM_LOOKUP(cpuid_get_feature_names);
	SYM_LOOKUP(cpuid_get_extfeature_names);
	SYM_LOOKUP(cpuid_get_leaf7_feature_names);
	SYM_LOOKUP(cpuid_get_leaf7_extfeature_names);

	// Vnode
	SYM_LOOKUP(vn_stat);
				   
	return KERN_SUCCESS;
}
