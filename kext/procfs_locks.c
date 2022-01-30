#include <kern/locks.h> 
#include <kern/lock_group.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/tty.h>

lck_attr_t *proc_lck_attr;
lck_grp_attr_t *proc_lck_grp_attr;
lck_grp_t *proc_mlock_grp;
lck_mtx_t *proc_list_mlock;

#pragma mark -
#pragma mark External References

extern void lck_mtx_lock_spin(lck_mtx_t *lck);

#pragma mark -
#pragma mark Local Function Prototypes

void procfs_list_lock(void);
void procfs_list_unlock(void);
void procfs_fdlock(proc_t p);
void procfs_fdlock_spin(proc_t p);
void procfs_fdunlock(proc_t p);
void procfs_pgrp_lock(struct pgrp * pgrp);
void procfs_pgrp_unlock(struct pgrp * pgrp);
void procfs_tty_lock(struct tty *tp);
void procfs_tty_unlock(struct tty *tp);

#pragma mark -
#pragma mark Procfs Locks

void
procfs_list_lock(void)
{
   proc_lck_attr = lck_attr_alloc_init();
   proc_lck_grp_attr = lck_grp_attr_alloc_init();
   proc_mlock_grp = lck_grp_alloc_init("proc-mlock", proc_lck_grp_attr);
   proc_list_mlock = lck_mtx_alloc_init(proc_mlock_grp, proc_lck_attr);

   lck_mtx_lock(proc_list_mlock);
}

void
procfs_list_unlock(void)
{
   proc_lck_attr = lck_attr_alloc_init();
   proc_lck_grp_attr = lck_grp_attr_alloc_init();
   proc_mlock_grp = lck_grp_alloc_init("proc-mlock", proc_lck_grp_attr);
   proc_list_mlock = lck_mtx_alloc_init(proc_mlock_grp, proc_lck_attr);

   lck_mtx_unlock(proc_list_mlock);
}

void
procfs_fdlock(proc_t p)
{
   lck_mtx_lock(&p->p_fdmlock);
}

void
procfs_fdlock_spin(proc_t p)
{
   lck_mtx_lock_spin(&p->p_fdmlock);
}

void
procfs_fdunlock(proc_t p)
{
   lck_mtx_unlock(&p->p_fdmlock);
}

void
procfs_pgrp_lock(struct pgrp * pgrp)
{
   lck_mtx_lock(&pgrp->pg_mlock);
}

void
procfs_pgrp_unlock(struct pgrp * pgrp)
{
   lck_mtx_unlock(&pgrp->pg_mlock);
}

void
procfs_tty_lock(struct tty *tp)
{
   lck_mtx_lock(&tp->t_lock);
}

void
procfs_tty_unlock(struct tty *tp)
{
   lck_mtx_unlock(&tp->t_lock);
}
