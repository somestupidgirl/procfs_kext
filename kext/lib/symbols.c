/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * symbols.c
 *
 * Private KPI symbol resolution via libklookup.
 * Stubbed out for ARM64 - libklookup uses a hardcoded
 * KERNEL_BASE that is invalid on ARM64 with KASLR,
 * causing a kernel panic on boot.
 *
 * TODO: Replace with a proper ARM64 kernel slide
 * detection mechanism.
 */
#include <mach/kern_return.h>

#include "symbols.h"

#define SYM_INIT(sym) \
	__typeof(_##sym) _##sym = NULL

SYM_INIT(initproc);
SYM_INIT(allproc);
SYM_INIT(nprocs);
SYM_INIT(maxproc);
SYM_INIT(maxprocperuid);
SYM_INIT(hard_maxproc);
SYM_INIT(proc_best_name);
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
SYM_INIT(forkproc);
SYM_INIT(forkproc_free);
SYM_INIT(filedesc0);
SYM_INIT(fp_getfvp);
SYM_INIT(fdcopy);
SYM_INIT(fdfree);
SYM_INIT(fdt_next);
SYM_INIT(fdt_prev);
SYM_INIT(proc_fdlist);
SYM_INIT(proc_fdlock);
SYM_INIT(proc_fdlock_assert);
SYM_INIT(proc_fdlock_spin);
SYM_INIT(proc_fdunlock);
SYM_INIT(fill_taskprocinfo);
SYM_INIT(fill_taskthreadinfo);
SYM_INIT(fill_socketinfo);
SYM_INIT(proc_task);
SYM_INIT(proc_thread);
SYM_INIT(thread_info);
SYM_INIT(get_bsdthread_info);
SYM_INIT(bsd_threadcdir);
SYM_INIT(uthread_alloc);
SYM_INIT(convert_port_to_thread);
SYM_INIT(task_threads);
SYM_INIT(vfs_context_thread);
SYM_INIT(vn_stat);
SYM_INIT(vnode_lock);
SYM_INIT(vnode_unlock);
SYM_INIT(dead_mountp);
SYM_INIT(processor_count);
SYM_INIT(avenrun);

kern_return_t
resolve_symbols(void)
{
	/* Stubbed: libklookup panics on ARM64 due to hardcoded KERNEL_BASE. */
	return KERN_SUCCESS;
}
