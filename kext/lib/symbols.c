/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * symbols.c
 *
 * This file holds a function for resolving given symbols.
 * It gets called only once during the kext's start routine.
 */
#include <libklookup/klookup.h>
#include <mach/kern_return.h>

#include "symbols.h"

#pragma mark -
#pragma mark Macros

#define SYM_INIT(sym) __typeof(_##sym) _##sym = NULL

#define SYM_LOOKUP(sym) { \
    if (!(_##sym = SymbolLookup("_"#sym))) { \
        return KERN_FAILURE; \
    } \
}

#pragma mark -
#pragma mark Main Routine

// Global Variables
SYM_INIT(initproc);
SYM_INIT(allproc);
SYM_INIT(nprocs);
SYM_INIT(maxproc);
SYM_INIT(maxprocperuid);
SYM_INIT(hard_maxproc);
SYM_INIT(processor_count);
SYM_INIT(tscFreq);
SYM_INIT(dead_mountp);

// Process Locks
SYM_INIT(proc_list_lock);
SYM_INIT(proc_list_unlock);
SYM_INIT(proc_fdlock);
SYM_INIT(proc_fdlock_spin);
SYM_INIT(proc_fdunlock);
SYM_INIT(vnode_lock);
SYM_INIT(vnode_unlock);

// Process Iteration
SYM_INIT(proc_iterate);

// Process Group and Session
SYM_INIT(proc_pgrp);
SYM_INIT(tty_pgrp);
SYM_INIT(proc_session);

// UID/GID
SYM_INIT(proc_issetugid);

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

// File Descriptor
SYM_INIT(proc_fdlist);
SYM_INIT(fdt_next);
SYM_INIT(fdcopy);
SYM_INIT(fdfree);

// Thread
SYM_INIT(convert_port_to_thread);
SYM_INIT(task_threads);
SYM_INIT(thread_info);

// CPU
SYM_INIT(cpuid_info);

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
	SYM_LOOKUP(dead_mountp);

	// Process Locks
	SYM_LOOKUP(proc_list_lock);
	SYM_LOOKUP(proc_list_unlock);
	SYM_LOOKUP(proc_fdlock);
	SYM_LOOKUP(proc_fdlock_spin);
	SYM_LOOKUP(proc_fdunlock);
	SYM_LOOKUP(vnode_lock);
	SYM_LOOKUP(vnode_unlock);

	// Process Iteration
	SYM_LOOKUP(proc_iterate);

	// Process Group and Session
	SYM_LOOKUP(proc_pgrp);
	SYM_LOOKUP(tty_pgrp);
	SYM_LOOKUP(proc_session);

	// UID/GID
	SYM_LOOKUP(proc_issetugid);

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

	// File Descriptor
	SYM_LOOKUP(proc_fdlist);
	SYM_LOOKUP(fdt_next);
	SYM_LOOKUP(fdcopy);
	SYM_LOOKUP(fdfree);

	// Thread
	SYM_LOOKUP(convert_port_to_thread);
	SYM_LOOKUP(task_threads);
	SYM_LOOKUP(thread_info);

	// CPU ID
	SYM_LOOKUP(cpuid_info);

	// Vnode
	SYM_LOOKUP(vn_stat);
				   
	return KERN_SUCCESS;
}
