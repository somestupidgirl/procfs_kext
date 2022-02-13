#include <os/base.h>

#include <kern/debug.h>
#include <kern/kalloc.h>
#include <kern/assert.h>
#include <kern/debug.h>
#include <kern/kalloc.h>
#include <kern/locks.h>
#include <kern/lock_group.h>

#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/vm.h>
#include <sys/vnode.h>

#pragma mark -
#pragma mark Local Definitions

#if !defined(__x86_64__)
#define NPROC 1000          /* Account for TOTAL_CORPSES_ALLOWED by making this slightly lower than we can. */
#define NPROC_PER_UID 950
#else
#define NPROC (20 + 32 * 32)
#define NPROC_PER_UID (NPROC/2)
#endif

/* NOTE: maxproc and hard_maxproc values are subject to device specific scaling in bsd_scale_setup */
#define HNPROC 2500     /* based on thread_max */
int maxproc = NPROC;
int maxprocperuid = NPROC_PER_UID;

#if !defined(__x86_64__)
int hard_maxproc = NPROC;       /* hardcoded limit -- for ARM the number of processes is limited by the ASID space */
#else
int hard_maxproc = HNPROC;      /* hardcoded limit */
#endif

int nprocs = 0; /* XXX */

struct pidhashhead *pidhashtbl;
u_long pidhash;

struct pgrphashhead *pgrphashtbl;
u_long pgrphash;

struct sesshashhead *sesshashtbl;
u_long sesshash;

struct proclist allproc;
struct proclist zombproc;

#pragma mark -
#pragma mark Pidlist Routines

/*
 * The pidlist_* routines support the functions in this file that
 * walk lists of processes applying filters and callouts to the
 * elements of the list.
 *
 * A prior implementation used a single linear array, which can be
 * tricky to allocate on large systems. This implementation creates
 * an SLIST of modestly sized arrays of PIDS_PER_ENTRY elements.
 *
 * The array should be sized large enough to keep the overhead of
 * walking the list low, but small enough that blocking allocations of
 * pidlist_entry_t structures always succeed.
 */

#define PIDS_PER_ENTRY 1021

typedef struct pidlist_entry {
    SLIST_ENTRY(pidlist_entry)  pe_link;
    u_int                       pe_nused;
    pid_t                       pe_pid[PIDS_PER_ENTRY];
} pidlist_entry_t;

typedef struct {
    SLIST_HEAD(, pidlist_entry) pl_head;
    struct pidlist_entry       *pl_active;
    u_int                       pl_nalloc;
} pidlist_t;

static __inline__ pidlist_t *
_pidlist_init(pidlist_t *pl)
{
    SLIST_INIT(&pl->pl_head);
    pl->pl_active = NULL;
    pl->pl_nalloc = 0;
    return pl;
}

static u_int
_pidlist_alloc(pidlist_t *pl, u_int needed)
{
    while (pl->pl_nalloc < needed) {
        pidlist_entry_t *pe = kalloc(sizeof(*pe));
        if (NULL == pe) {
            panic("no space for pidlist entry");
        }
        SLIST_INSERT_HEAD(&pl->pl_head, pe, pe_link);
        pl->pl_nalloc += (sizeof(pe->pe_pid) / sizeof(pe->pe_pid[0]));
    }
    return pl->pl_nalloc;
}

static void
_pidlist_free(pidlist_t *pl)
{
    pidlist_entry_t *pe;
    while (NULL != (pe = SLIST_FIRST(&pl->pl_head))) {
        SLIST_FIRST(&pl->pl_head) = SLIST_NEXT(pe, pe_link);
        kalloc(sizeof(*pe));
    }
    pl->pl_nalloc = 0;
}

static __inline__ void
_pidlist_set_active(pidlist_t *pl)
{
    pl->pl_active = SLIST_FIRST(&pl->pl_head);
    assert(pl->pl_active);
}

static void
_pidlist_add_pid(pidlist_t *pl, pid_t pid)
{
    pidlist_entry_t *pe = pl->pl_active;
    if (pe->pe_nused >= sizeof(pe->pe_pid) / sizeof(pe->pe_pid[0])) {
        if (NULL == (pe = SLIST_NEXT(pe, pe_link))) {
            panic("pidlist allocation exhausted");
        }
        pl->pl_active = pe;
    }
    pe->pe_pid[pe->pe_nused++] = pid;
}

static u_int
_pidlist_nalloc(const pidlist_t *pl)
{
    return pl->pl_nalloc;
}

#pragma mark -
#pragma mark Mutex Locks

extern void lck_mtx_lock_spin(lck_mtx_t *lck);

lck_mtx_t * proc_list_mlock;
lck_mtx_t * proc_klist_mlock;
lck_grp_t * proc_lck_grp;
lck_grp_t * proc_fdmlock_grp;
lck_grp_t * proc_kqhashlock_grp;
lck_grp_t * proc_knhashlock_grp;
lck_grp_t * proc_mlock_grp;
lck_grp_t * proc_ucred_mlock_grp;
lck_grp_t * proc_slock_grp;
lck_grp_t * proc_dirslock_grp;
lck_grp_attr_t * proc_lck_grp_attr;
lck_attr_t * proc_lck_attr;

void
_proc_lock(proc_t p)
{
    LCK_MTX_ASSERT(proc_list_mlock, LCK_MTX_ASSERT_NOTOWNED);
    lck_mtx_lock(&p->p_mlock);
}

void
_proc_unlock(proc_t p)
{
    lck_mtx_unlock(&p->p_mlock);
}

void
_proc_spinlock(proc_t p)
{
    lck_spin_lock(&p->p_slock);
}

void
_proc_spinunlock(proc_t p)
{
    lck_spin_unlock(&p->p_slock);
}

void 
_proc_list_lock(void)
{
    lck_mtx_lock(proc_list_mlock);
}

void 
_proc_list_unlock(void)
{
    lck_mtx_unlock(proc_list_mlock);
}

void
_proc_klist_lock(void)
{
    lck_mtx_lock(proc_klist_mlock);
}

void
_proc_klist_unlock(void)
{
    lck_mtx_unlock(proc_klist_mlock);
}

void 
_proc_ucred_lock(proc_t p)
{
    lck_mtx_lock(&p->p_ucred_mlock);
}

void 
_proc_ucred_unlock(proc_t p)
{
    lck_mtx_unlock(&p->p_ucred_mlock);
}

/*
 * proc_fdlock, proc_fdlock_spin
 *
 * Description: Lock to control access to the per process struct fileproc
 *      and struct filedesc
 *
 * Parameters:  p               Process to take the lock on
 *
 * Returns: void
 *
 * Notes:   The lock is initialized in forkproc() and destroyed in
 *      reap_child_process().
 */
void
_proc_fdlock(proc_t p)
{
    lck_mtx_lock(&p->p_fdmlock);
}

void
_proc_fdlock_spin(proc_t p)
{
    lck_mtx_lock_spin(&p->p_fdmlock);
}

void
_proc_fdlock_assert(proc_t p, int assertflags)
{
    lck_mtx_assert(&p->p_fdmlock, assertflags);
}

/*
 * proc_fdunlock
 *
 * Description: Unlock the lock previously locked by a call to proc_fdlock()
 *
 * Parameters:  p               Process to drop the lock on
 *
 * Returns: void
 */
void
_proc_fdunlock(proc_t p)
{
    lck_mtx_unlock(&p->p_fdmlock);
}

void
_pgrp_lock(struct pgrp * pgrp)
{
    lck_mtx_lock(&pgrp->pg_mlock);
}

void
_pgrp_unlock(struct pgrp * pgrp)
{
    lck_mtx_unlock(&pgrp->pg_mlock);
}

void
_session_lock(struct session * sess)
{
    lck_mtx_lock(&sess->s_mlock);
}

void
_session_unlock(struct session * sess)
{
    lck_mtx_unlock(&sess->s_mlock);
}

/*
 * tty_lock
 *
 * Lock the requested tty structure.
 *
 * Parameters:  tp              The tty we want to lock
 *
 * Returns: void
 *
 * Locks:   On return, tp is locked
 */
void
_tty_lock(struct tty * tp)
{
    lck_mtx_lock(&tp->t_lock);
}

/*
 * tty_unlock
 *
 * Unlock the requested tty structure.
 *
 * Parameters:  tp              The tty we want to unlock
 *
 * Returns: void
 *
 * Locks:   On return, tp is unlocked
 */
void
_tty_unlock(struct tty * tp)
{
    lck_mtx_unlock(&tp->t_lock);
}

#pragma mark -
#pragma mark Process Group Functions

struct pgrp *
_proc_pgrp(proc_t p)
{
    struct pgrp * pgrp;

    if (p == PROC_NULL) {
        return PGRP_NULL;
    }
    _proc_list_lock();

    while ((p->p_listflag & P_LIST_PGRPTRANS) == P_LIST_PGRPTRANS) {
        p->p_listflag |= P_LIST_PGRPTRWAIT;
        (void)msleep(&p->p_pgrpid, &proc_list_mlock, 0, "proc_pgrp", 0);
    }

    pgrp = p->p_pgrp;

    assert(pgrp != NULL);

    if (pgrp != PGRP_NULL) {
        pgrp->pg_refcount++;
        if ((pgrp->pg_listflags & (PGRP_FLAG_TERMINATE | PGRP_FLAG_DEAD)) != 0) {
            panic("proc_pgrp: ref being povided for dead pgrp");
        }
    }

    _proc_list_unlock();

    return pgrp;
}

/*
 * delete a process group
 */
static void
_pgdelete_dropref(struct pgrp *pgrp)
{
    struct tty *ttyp;
    int emptypgrp  = 1;
    struct session *sessp;


    _pgrp_lock(pgrp);
    if (pgrp->pg_membercnt != 0) {
        emptypgrp = 0;
    }
    _pgrp_unlock(pgrp);

    _proc_list_lock();
    pgrp->pg_refcount--;
    if ((emptypgrp == 0) || (pgrp->pg_membercnt != 0)) {
        _proc_list_unlock();
        return;
    }

    pgrp->pg_listflags |= PGRP_FLAG_TERMINATE;

    if (pgrp->pg_refcount > 0) {
        _proc_list_unlock();
        return;
    }

    pgrp->pg_listflags |= PGRP_FLAG_DEAD;
    LIST_REMOVE(pgrp, pg_hash);

    _proc_list_unlock();

    ttyp = SESSION_TP(pgrp->pg_session);
    if (ttyp != TTY_NULL) {
        if (ttyp->t_pgrp == pgrp) {
            _tty_lock(ttyp);
            /* Re-check after acquiring the lock */
            if (ttyp->t_pgrp == pgrp) {
                ttyp->t_pgrp = NULL;
                pgrp->pg_session->s_ttypgrpid = NO_PID;
            }
            _tty_unlock(ttyp);
        }
    }

    _proc_list_lock();

    sessp = pgrp->pg_session;
    if ((sessp->s_listflags & (S_LIST_TERM | S_LIST_DEAD)) != 0) {
        panic("pg_deleteref: manipulating refs of already terminating session");
    }
    if (--sessp->s_count == 0) {
        if ((sessp->s_listflags & (S_LIST_TERM | S_LIST_DEAD)) != 0) {
            panic("pg_deleteref: terminating already terminated session");
        }
        sessp->s_listflags |= S_LIST_TERM;
        ttyp = SESSION_TP(sessp);
        LIST_REMOVE(sessp, s_hash);
        _proc_list_unlock();
        if (ttyp != TTY_NULL) {
            _tty_lock(ttyp);
            if (ttyp->t_session == sessp) {
                ttyp->t_session = NULL;
            }
            _tty_unlock(ttyp);
        }
        _proc_list_lock();
        sessp->s_listflags |= S_LIST_DEAD;
        if (sessp->s_count != 0) {
            panic("pg_deleteref: freeing session in use");
        }
        _proc_list_unlock();
        lck_mtx_destroy(&sessp->s_mlock, &proc_mlock_grp);

        _FREE_ZONE(sessp, sizeof(struct session), M_ZERO);
    } else {
        _proc_list_unlock();
    }
    lck_mtx_destroy(&pgrp->pg_mlock, &proc_mlock_grp);
    _FREE_ZONE(pgrp, sizeof(struct pgrp), M_ZERO);
}

static void
_pg_rele_dropref(struct pgrp * pgrp)
{
    _proc_list_lock();
    if ((pgrp->pg_refcount == 1) && ((pgrp->pg_listflags & PGRP_FLAG_TERMINATE) == PGRP_FLAG_TERMINATE)) {
        _proc_list_unlock();
        _pgdelete_dropref(pgrp);
        return;
    }

    pgrp->pg_refcount--;
    _proc_list_unlock();
}

void
_pg_rele(struct pgrp * pgrp)
{
    if (pgrp == PGRP_NULL) {
        return;
    }
    _pg_rele_dropref(pgrp);
}

void
_pgrp_iterate(struct pgrp *pgrp, unsigned int flags, proc_iterate_fn_t callout, void * arg, proc_iterate_fn_t filterfn, void * filterarg)
{
    pidlist_t pid_list, *pl = _pidlist_init(&pid_list);
    u_int pid_count_available = 0;

    assert(pgrp != NULL);
    assert(callout != NULL);

    for (;;) {
        _pgrp_lock(pgrp);
        pid_count_available = pgrp->pg_membercnt;
        if (pid_count_available == 0) {
            _pgrp_unlock(pgrp);
            if (flags & PGRP_DROPREF) {
                _pg_rele(pgrp);
            }
            goto out;
        }
        if (_pidlist_nalloc(pl) >= pid_count_available) {
            break;
        }
        _pgrp_unlock(pgrp);

        _pidlist_alloc(pl, pid_count_available);
    }
    _pidlist_set_active(pl);

    const pid_t pgid = pgrp->pg_id;
    u_int pid_count = 0;
    proc_t p;
    PGMEMBERS_FOREACH(pgrp, p) {
        if ((filterfn != NULL) && (filterfn(p, filterarg) == 0)) {
            continue;;
        }
        _pidlist_add_pid(pl, proc_pid(p));
        if (++pid_count >= pid_count_available) {
            break;
        }
    }

    _pgrp_unlock(pgrp);

    if (flags & PGRP_DROPREF) {
        _pg_rele(pgrp);
    }

    const pidlist_entry_t *pe;
    SLIST_FOREACH(pe, &(pl->pl_head), pe_link) {
        for (u_int i = 0; i < pe->pe_nused; i++) {
            const pid_t pid = pe->pe_pid[i];
            if (0 == pid) {
                continue; /* skip kernproc */
            }
            p = proc_find(pid);
            if (!p) {
                continue;
            }
            if (p->p_pgrpid != pgid) {
                proc_rele(p);
                continue;
            }
            const int callout_ret = callout(p, arg);

            switch (callout_ret) {
            case PROC_RETURNED:
                proc_rele(p);
                OS_FALLTHROUGH;
            case PROC_CLAIMED:
                break;
            case PROC_RETURNED_DONE:
                proc_rele(p);
                OS_FALLTHROUGH;
            case PROC_CLAIMED_DONE:
                goto out;

            default:
                panic("%s: callout =%d for pid %d",
                    __func__, callout_ret, pid);
            }
        }
    }

out:
    _pidlist_free(pl);
}

#pragma mark -
#pragma mark Session Functions

struct session *
_proc_session(proc_t p)
{
    struct session * sess = SESSION_NULL;

    if (p == PROC_NULL) {
        return SESSION_NULL;
    }

    _proc_list_lock();

    /* wait during transitions */
    while ((p->p_listflag & P_LIST_PGRPTRANS) == P_LIST_PGRPTRANS) {
        p->p_listflag |= P_LIST_PGRPTRWAIT;
        (void)msleep(&p->p_pgrpid, &proc_list_mlock, 0, "proc_pgrp", 0);
    }

    if ((p->p_pgrp != PGRP_NULL) && ((sess = p->p_pgrp->pg_session) != SESSION_NULL)) {
        if ((sess->s_listflags & (S_LIST_TERM | S_LIST_DEAD)) != 0) {
            panic("proc_session:returning sesssion ref on terminating session");
        }
        sess->s_count++;
    }
    _proc_list_unlock();
    return sess;
}

void
_session_rele(struct session *sess)
{
    _proc_list_lock();
    if (--sess->s_count == 0) {
        if ((sess->s_listflags & (S_LIST_TERM | S_LIST_DEAD)) != 0) {
            panic("session_rele: terminating already terminated session");
        }
        sess->s_listflags |= S_LIST_TERM;
        LIST_REMOVE(sess, s_hash);
        sess->s_listflags |= S_LIST_DEAD;
        if (sess->s_count != 0) {
            panic("session_rele: freeing session in use");
        }
        _proc_list_unlock();
        lck_mtx_destroy(&sess->s_mlock, &proc_mlock_grp);
        _FREE_ZONE(sess, sizeof(struct session), M_ZERO);
    } else {
        _proc_list_unlock();
    }
}

#pragma mark -
#pragma mark Proc Functions

int
_proc_transstart(proc_t p, int locked, int non_blocking)
{
    if (locked == 0) {
        _proc_lock(p);
    }
    while ((p->p_lflag & P_LINTRANSIT) == P_LINTRANSIT) {
        if (((p->p_lflag & P_LTRANSCOMMIT) == P_LTRANSCOMMIT) || non_blocking) {
            if (locked == 0) {
                _proc_unlock(p);
            }
            return EDEADLK;
        }
        p->p_lflag |= P_LTRANSWAIT;
        msleep(&p->p_lflag, &p->p_mlock, 0, "proc_signstart", NULL);
    }
    p->p_lflag |= P_LINTRANSIT;
    p->p_transholder = current_thread();
    if (locked == 0) {
        _proc_unlock(p);
    }
    return 0;
}

void
_proc_transcommit(proc_t p, int locked)
{
    if (locked == 0) {
        _proc_lock(p);
    }

    assert((p->p_lflag & P_LINTRANSIT) == P_LINTRANSIT);
    assert(p->p_transholder == current_thread());
    p->p_lflag |= P_LTRANSCOMMIT;

    if ((p->p_lflag & P_LTRANSWAIT) == P_LTRANSWAIT) {
        p->p_lflag &= ~P_LTRANSWAIT;
        wakeup(&p->p_lflag);
    }
    if (locked == 0) {
        _proc_unlock(p);
    }
}

void
_proc_transend(proc_t p, int locked)
{
    if (locked == 0) {
        _proc_lock(p);
    }

    p->p_lflag &= ~(P_LINTRANSIT | P_LTRANSCOMMIT);
    p->p_transholder = NULL;

    if ((p->p_lflag & P_LTRANSWAIT) == P_LTRANSWAIT) {
        p->p_lflag &= ~P_LTRANSWAIT;
        wakeup(&p->p_lflag);
    }
    if (locked == 0) {
        _proc_unlock(p);
    }
}

int
_proc_transwait(proc_t p, int locked)
{
    if (locked == 0) {
        _proc_list_lock();
    }
    while ((p->p_lflag & P_LINTRANSIT) == P_LINTRANSIT) {
        if ((p->p_lflag & P_LTRANSCOMMIT) == P_LTRANSCOMMIT && current_proc() == p) {
            if (locked == 0) {
                _proc_list_unlock();
            }
            return EDEADLK;
        }
        p->p_lflag |= P_LTRANSWAIT;
        msleep(&p->p_lflag, &p->p_mlock, 0, "proc_signstart", NULL);
    }
    if (locked == 0) {
        _proc_list_unlock();
    }
    return 0;
}

/*
 * Locate a process by number
 */
proc_t
_pfind_locked(pid_t pid)
{
    proc_t p;

    if (!pid) {
        return kernproc;
    }

    for (p = PIDHASH(pid)->lh_first; p != 0; p = p->p_hash.le_next) {
        if (proc_pid(p) == pid) {
            return p;
        }
    }
    return NULL;
}

/*
 * Locate a zombie process reference
 */
proc_t
_proc_find_zombref(int pid)
{
    proc_t p;

    _proc_list_lock();

again:
    p = _pfind_locked(pid);

    /* should we bail? */
    if ((p == PROC_NULL)                                    /* not found */
        || ((p->p_listflag & P_LIST_INCREATE) != 0)         /* not created yet */
        || ((p->p_listflag & P_LIST_EXITED) == 0)) {        /* not started exit */
        _proc_list_unlock();
        return PROC_NULL;
    }

    /* If someone else is controlling the (unreaped) zombie - wait */
    if ((p->p_listflag & P_LIST_WAITING) != 0) {
        (void)msleep(&p->p_stat, proc_list_mlock, PWAIT, "waitcoll", 0);
        goto again;
    }
    p->p_listflag |=  P_LIST_WAITING;

    _proc_list_unlock();

    return p;
}

/*
 * Drop a zombie process reference
 */
void
_proc_drop_zombref(proc_t p)
{
    _proc_list_lock();
    if ((p->p_listflag & P_LIST_WAITING) == P_LIST_WAITING) {
        p->p_listflag &= ~P_LIST_WAITING;
        wakeup(&p->p_stat);
    }
    _proc_list_unlock();
}

void
_proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg)
{
    uint32_t nprocs = 0;

    pidlist_t pid_list, *pl = _pidlist_init(&pid_list);
    u_int pid_count_available = 0;

    assert(callout != NULL);

    /* allocate outside of the proc_list_lock */
    for (;;) {
        _proc_list_lock();
        pid_count_available = nprocs + 1; /* kernel_task not counted in nprocs */
        assert(pid_count_available > 0);
        if (_pidlist_nalloc(pl) > pid_count_available) {
            break;
        }
        _proc_list_unlock();

        _pidlist_alloc(pl, pid_count_available);
    }
    _pidlist_set_active(pl);

    /* filter pids into the pid_list */
    u_int pid_count = 0;
    if (flags & PROC_ALLPROCLIST) {
        proc_t p;
        ALLPROC_FOREACH(p) {
            /* ignore processes that are being forked */
            if (p->p_stat == 1) {
                continue;
            }
            if ((filterfn != NULL) && (filterfn(p, filterarg) == 0)) {
                continue;
            }
        }
    }

    if ((pid_count < pid_count_available) &&
        (flags & PROC_ZOMBPROCLIST)) {
        proc_t p;
        ZOMBPROC_FOREACH(p) {
            if ((filterfn != NULL) && (filterfn(p, filterarg) == 0)) {
                continue;
            }
        }
    }
    _proc_list_unlock();

    /* call callout on processes in the pid_list */
    const pidlist_entry_t *pe;
    SLIST_FOREACH(pe, &(pl->pl_head), pe_link) {
        for (u_int i = 0; i < pe->pe_nused; i++) {
            const pid_t pid = pe->pe_pid[i];
            proc_t p = proc_find(pid);
            if (p) {
                if ((flags & PROC_NOWAITTRANS) == 0) {
                    _proc_transwait(p, 0);
                }
                const int callout_ret = callout(p, arg);

                switch (callout_ret) {
                case PROC_RETURNED_DONE:
                    proc_rele(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED_DONE:
                    goto out;

                case PROC_RETURNED:
                    proc_rele(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED:
                    break;
                default:
                    panic("%s: callout =%d for pid %d",
                        __func__, callout_ret, pid);
                    break;
                }
            } else if (flags & PROC_ZOMBPROCLIST) {
                p = _proc_find_zombref(pid);
                if (!p) {
                    continue;
                }
                const int callout_ret = callout(p, arg);

                switch (callout_ret) {
                case PROC_RETURNED_DONE:
                    _proc_drop_zombref(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED_DONE:
                    goto out;

                case PROC_RETURNED:
                    _proc_drop_zombref(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED:
                    break;
                default:
                    panic("%s: callout =%d for zombie %d",
                        __func__, callout_ret, pid);
                    break;
                }
            }
        }
    }
out:
    _pidlist_free(pl);
}

task_t
_proc_task(proc_t proc)
{
    return (task_t)proc->task;
}
