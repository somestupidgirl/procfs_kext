/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * symbols.c
 *
 * This file holds a function for resolving private KPI symbols.
 */
#include <libklookup/klookup.h>
#include <mach/kern_return.h>

#include "symbols.h"

/*
 * Declares a symbol, initializing it with NULL.
 * This must be done before we resolve them in
 * the resolve_symbols() routine below.
 *
 * They receive their respective types from the
 * symbols.h header. 
 */
#define SYM_INIT(sym) \
	__typeof(_##sym) _##sym = NULL

/*
 * Defines a symbol using the SymbolLookup()
 * function.
 *
 * Caution: If a symbol can't be resolved it
 * will result in a panic on system startup.
 * This should get fixed in the future.
 */
#define SYM_LOOKUP(sym) { \
    if (!(_##sym = SymbolLookup("_"#sym))) { \
        return KERN_FAILURE; \
    } \
}

/*
 * Initialize our symbols.
 */
SYM_INIT(initproc);
SYM_INIT(allproc);
SYM_INIT(nprocs);
SYM_INIT(maxproc);
SYM_INIT(maxprocperuid);
SYM_INIT(hard_maxproc);

SYM_INIT(proc_best_name);
SYM_INIT(proc_task);
SYM_INIT(proc_starttime);
SYM_INIT(proc_issetugid);
SYM_INIT(proc_get_darwinbgstate);
SYM_INIT(psignal);
SYM_INIT(tsleep);
SYM_INIT(proc_lock);
SYM_INIT(proc_unlock);
SYM_INIT(proc_list_lock);
SYM_INIT(proc_list_unlock);
SYM_INIT(proc_iterate);

SYM_INIT(proc_pgrp);
SYM_INIT(pg_rele);
SYM_INIT(pgfind);
SYM_INIT(pgrp_lock);
SYM_INIT(pgrp_unlock);
SYM_INIT(pgrp_iterate);

SYM_INIT(proc_session);
SYM_INIT(session_rele);
SYM_INIT(session_lock);
SYM_INIT(session_unlock);

SYM_INIT(proc_gettty);
SYM_INIT(proc_gettty_dev);
SYM_INIT(tty_lock);
SYM_INIT(tty_unlock);

SYM_INIT(proc_fdlist);
SYM_INIT(fdalloc);
SYM_INIT(fdt_next);
SYM_INIT(fdt_prev);
SYM_INIT(fdcopy);
SYM_INIT(fdfree);
SYM_INIT(proc_fdlock);
SYM_INIT(proc_fdlock_assert);
SYM_INIT(proc_fdlock_spin);
SYM_INIT(proc_fdunlock);

SYM_INIT(fill_taskprocinfo);
SYM_INIT(fill_taskthreadinfo);
SYM_INIT(fill_socketinfo);

SYM_INIT(thread_info);
SYM_INIT(get_bsdthread_info);
SYM_INIT(convert_port_to_thread);
SYM_INIT(task_threads);
SYM_INIT(vfs_context_thread);

SYM_INIT(vn_stat);
SYM_INIT(vnode_lock);
SYM_INIT(vnode_unlock);

SYM_INIT(dead_mountp);

SYM_INIT(processor_count);
SYM_INIT(tscFreq);
SYM_INIT(cpuid_info);
SYM_INIT(compute_averunnable);

/*
 * Routine: resolve_symbols()
 *
 * Called only once by procfs_start() to resolve
 * private KPI symbols on kext startup. This allows
 * us to call them like normal functions without
 * having to call SymbolLookup() at the start of
 * every function that requires the private KPI.
 *
 * When adding new symbols they must also be added
 * to the symbols.h header.
 *
 * Return values:
 *
 *		KERN_SUCCESS (0) if successful.
 *		KERN_FAILURE (5) if unsuccessful.
 */
kern_return_t
resolve_symbols(void)
{
	SYM_LOOKUP(initproc);
	SYM_LOOKUP(allproc);
	SYM_LOOKUP(nprocs);
	SYM_LOOKUP(maxproc);
	SYM_LOOKUP(maxprocperuid);
	SYM_LOOKUP(hard_maxproc);

	SYM_LOOKUP(proc_best_name);
	SYM_LOOKUP(proc_task);
	SYM_LOOKUP(proc_starttime);
	SYM_LOOKUP(proc_issetugid);
	SYM_LOOKUP(proc_get_darwinbgstate);
	SYM_LOOKUP(psignal);
	SYM_LOOKUP(tsleep);
	SYM_LOOKUP(proc_lock);
	SYM_LOOKUP(proc_unlock);
	SYM_LOOKUP(proc_list_lock);
	SYM_LOOKUP(proc_list_unlock);
	SYM_LOOKUP(proc_iterate);

	SYM_LOOKUP(proc_pgrp);
	SYM_LOOKUP(pg_rele);
	SYM_LOOKUP(pgfind);
	SYM_LOOKUP(pgrp_lock);
	SYM_LOOKUP(pgrp_unlock);
	SYM_LOOKUP(pgrp_iterate);

	SYM_LOOKUP(proc_session);
	SYM_LOOKUP(session_rele);
	SYM_LOOKUP(session_lock);
	SYM_LOOKUP(session_unlock);

	SYM_LOOKUP(proc_gettty);
	SYM_LOOKUP(proc_gettty_dev);
	SYM_LOOKUP(tty_lock);
	SYM_LOOKUP(tty_unlock);

	SYM_LOOKUP(proc_fdlist);
	SYM_LOOKUP(fdalloc);
	SYM_LOOKUP(fdt_next);
	SYM_LOOKUP(fdt_prev);
	SYM_LOOKUP(fdcopy);
	SYM_LOOKUP(fdfree);
	SYM_LOOKUP(proc_fdlock);
	SYM_LOOKUP(proc_fdlock_assert);
	SYM_LOOKUP(proc_fdlock_spin);
	SYM_LOOKUP(proc_fdunlock);

	SYM_LOOKUP(fill_taskprocinfo);
	SYM_LOOKUP(fill_taskthreadinfo);
	SYM_LOOKUP(fill_socketinfo);

	SYM_LOOKUP(thread_info);
	SYM_LOOKUP(task_threads);
	SYM_LOOKUP(convert_port_to_thread);
	SYM_LOOKUP(vfs_context_thread);
	SYM_LOOKUP(get_bsdthread_info);

	SYM_LOOKUP(vn_stat);
	SYM_LOOKUP(vnode_lock);
	SYM_LOOKUP(vnode_unlock);

	SYM_LOOKUP(dead_mountp);

	SYM_LOOKUP(processor_count);
	SYM_LOOKUP(tscFreq);
	SYM_LOOKUP(cpuid_info);
	SYM_LOOKUP(compute_averunnable);
				   
	return KERN_SUCCESS;
}
