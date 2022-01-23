#include <kern/locks.h>
#include <sys/kernel_types.h>
#include <sys/proc_internal.h>

#include "procfs_locks.h"

extern void lck_mtx_lock_spin(lck_mtx_t *lck);

void
proc_fdlock(proc_t p)
{
	lck_mtx_lock(p->p_fdmlock);
}

void
proc_fdunlock(proc_t p)
{
	lck_mtx_unlock(p->p_fdmlock);
}

void
proc_fdlock_spin(proc_t p)
{
	lck_mtx_lock_spin(p->p_fdmlock);
}

void 
proc_list_lock(void)
{
	proc_t p = NULL;
	lck_mtx_lock(p->p_mlock);
}

void 
proc_list_unlock(void)
{
	proc_t p = NULL;
	lck_mtx_unlock(p->p_mlock);
}
