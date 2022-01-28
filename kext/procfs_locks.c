#include <kern/locks.h> 
#include <kern/lock_group.h>

#include <sys/proc.h>
#include <sys/proc_internal.h>

extern void lck_mtx_lock_spin(lck_mtx_t *lck);

void
proc_list_lock(void)
{
    lck_attr_t *proc_lck_attr = lck_attr_alloc_init();
    lck_grp_attr_t *proc_lck_grp_attr = lck_grp_attr_alloc_init();
    lck_grp_t *proc_mlock_grp = lck_grp_alloc_init("proc-mlock", proc_lck_grp_attr);
    lck_mtx_t *proc_list_mlock = lck_mtx_alloc_init(proc_mlock_grp, proc_lck_attr);
    lck_mtx_lock(proc_list_mlock);
} 

void
proc_list_unlock(void)
{
    lck_attr_t *proc_lck_attr = lck_attr_alloc_init();
    lck_grp_attr_t *proc_lck_grp_attr = lck_grp_attr_alloc_init();
    lck_grp_t *proc_mlock_grp = lck_grp_alloc_init("proc-mlock", proc_lck_grp_attr);
    lck_mtx_t *proc_list_mlock = lck_mtx_alloc_init(proc_mlock_grp, proc_lck_attr);
    lck_mtx_unlock(proc_list_mlock);
} 

void
proc_fdlock(proc_t p)
{
    lck_mtx_lock(&p->p_fdmlock);
} 

void
proc_fdlock_spin(proc_t p)
{
    lck_mtx_lock_spin(&p->p_fdmlock);
}

void
proc_fdunlock(proc_t p)
{
    lck_mtx_unlock(&p->p_fdmlock);
}
