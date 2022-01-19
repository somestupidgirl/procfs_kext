#include <kern/locks.h>
#include <sys/kernel_types.h>

#include "procfs_internal.h"
#include "procfs_locks.h"

extern void lck_mtx_lock_spin(lck_mtx_t *lck);

void
procfs_fdlock(proc_t p)
{
	lck_mtx_lock(p->p_fdmlock);
}

void
procfs_fdunlock(proc_t p)
{
	lck_mtx_unlock(p->p_fdmlock);
}

void
procfs_fdlock_spin(proc_t p)
{
	lck_mtx_lock_spin(p->p_fdmlock);
}

void 
procfs_list_lock(void)
{
	proc_t p = NULL;
	lck_mtx_lock(p->p_mlock);
}

void 
procfs_list_unlock(void)
{
	proc_t p = NULL;
	lck_mtx_unlock(p->p_mlock);
}
