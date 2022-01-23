#include <os/base.h>

#include <kern/debug.h>
#include <kern/kalloc.h>
#include <kern/zalloc.h>

#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/queue.h>

#include "procfs_proc.h"
#include "procfs_locks.h"

lck_grp_t * proc_mlock_grp;

LIST_HEAD(proclist, proc);
struct proclist allproc;         /* List of all processes. */
struct proclist zombproc;        /* List of zombie processes. */

#define PIDHASH(pid)    (&pidhashtbl[(pid) & pidhash])
LIST_HEAD(pidhashhead, proc) * pidhashtbl;
u_long pidhash;

#pragma mark -
#pragma mark Structures and Type Definitions

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
#define PIDS_PER_ENTRY      1021

typedef struct pidlist_entry {
    SLIST_ENTRY(pidlist_entry) pe_link;
    u_int pe_nused;
    pid_t pe_pid[PIDS_PER_ENTRY];
} pidlist_entry_t;

typedef struct {
    SLIST_HEAD(, pidlist_entry) pl_head;
    struct pidlist_entry *pl_active;
    u_int pl_nalloc;
} pidlist_t;


#pragma mark -
#pragma mark External References

extern struct proc *current_proc(void);


#pragma mark -
#pragma mark Static Inline Functions

static __inline__ pidlist_t *
pidlist_init(pidlist_t *pl)
{
	SLIST_INIT(&pl->pl_head);
	pl->pl_active = NULL;
	pl->pl_nalloc = 0;
	return pl;
}

static __inline__ u_int
pidlist_alloc(pidlist_t *pl, u_int needed)
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

static __inline__ u_int
pidlist_nalloc(const pidlist_t *pl)
{
	return pl->pl_nalloc;
}

static __inline__ void
pidlist_set_active(pidlist_t *pl)
{
	pl->pl_active = SLIST_FIRST(&pl->pl_head);
	assert(pl->pl_active);
}

static __inline__ void
pidlist_free(pidlist_t *pl)
{
    pidlist_entry_t *pe;
    while (NULL != (pe = SLIST_FIRST(&pl->pl_head))) {
        SLIST_FIRST(&pl->pl_head) = SLIST_NEXT(pe, pe_link);
        kalloc(sizeof(*pe));
    }
    pl->pl_nalloc = 0;
}

static __inline__ int
proc_transwait(proc_t p, int locked)
{
	if (locked == 0) {
		proc_list_lock();
	}
	while ((p->p_lflag & P_LINTRANSIT) == P_LINTRANSIT) {
		if ((p->p_lflag & P_LTRANSCOMMIT) == P_LTRANSCOMMIT && current_proc() == p) {
			if (locked == 0) {
				proc_list_unlock();
			}
			return EDEADLK;
		}
		p->p_lflag |= P_LTRANSWAIT;
		msleep(&p->p_lflag, &p->p_mlock, 0, "proc_signstart", NULL);
	}
	if (locked == 0) {
		proc_list_unlock();
	}
	return 0;
}

/*
 * Locate a process by number
 */
static __inline__ proc_t
pfind_locked(pid_t pid)
{
    proc_t p;

    if (!pid) {
        return kernproc;
    }

    for (p = PIDHASH(pid)->lh_first; p != 0; p = p->p_hash.le_next) {
        if (p->p_pid == pid) {
            return p;
        }
    }
    return NULL;
}


#pragma mark -
#pragma mark Global Functions

proc_t
proc_find_zombref(int pid)
{
    proc_t p;

    proc_list_lock();

again:
    p = pfind_locked(pid);

    /* should we bail? */
    if ((p == PROC_NULL)                                    /* not found */
        || ((p->p_listflag & P_LIST_INCREATE) != 0)         /* not created yet */
        || ((p->p_listflag & P_LIST_EXITED) == 0)) {        /* not started exit */
        proc_list_unlock();
        return PROC_NULL;
    }

    /* If someone else is controlling the (unreaped) zombie - wait */
    if ((p->p_listflag & P_LIST_WAITING) != 0) {
        lck_mtx_t *proc_list_mlock;
        (void)msleep(&p->p_stat, proc_list_mlock, PWAIT, "waitcoll", 0);
        goto again;
    }
    p->p_listflag |=  P_LIST_WAITING;

    proc_list_unlock();

    return p;
}

void
proc_drop_zombref(proc_t p)
{
    proc_list_lock();
    if ((p->p_listflag & P_LIST_WAITING) == P_LIST_WAITING) {
        p->p_listflag &= ~P_LIST_WAITING;
        wakeup(&p->p_stat);
    }
    proc_list_unlock();
}

void
session_rele(struct session *sess)
{
    proc_list_lock();
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
        proc_list_unlock();
        lck_mtx_destroy(&sess->s_mlock, proc_mlock_grp);
        //zfree(session_zone, sess);
    } else {
        proc_list_unlock();
    }
}

void
proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg)
{
	uint32_t nprocs = 0;

	pidlist_t pid_list, *pl = pidlist_init(&pid_list);
	u_int pid_count_available = 0;

	assert(callout != NULL);

	/* allocate outside of the proc_list_lock */
	for (;;) {
		proc_list_lock();
		pid_count_available = nprocs + 1; /* kernel_task not counted in nprocs */
		assert(pid_count_available > 0);
		if (pidlist_nalloc(pl) > pid_count_available) {
			break;
		}
		proc_list_unlock();

		pidlist_alloc(pl, pid_count_available);
	}
	pidlist_set_active(pl);

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
	proc_list_unlock();

	/* call callout on processes in the pid_list */
	const pidlist_entry_t *pe;
    SLIST_FOREACH(pe, &(pl->pl_head), pe_link) {
        for (u_int i = 0; i < pe->pe_nused; i++) {
            const pid_t pid = pe->pe_pid[i];
            proc_t p = proc_find(pid);
            if (p) {
                if ((flags & PROC_NOWAITTRANS) == 0) {
                    proc_transwait(p, 0);
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
                p = proc_find_zombref(pid);
                if (!p) {
                    continue;
                }
                const int callout_ret = callout(p, arg);

                switch (callout_ret) {
                case PROC_RETURNED_DONE:
                    proc_drop_zombref(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED_DONE:
                    goto out;

                case PROC_RETURNED:
                    proc_drop_zombref(p);
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
    pidlist_free(pl);
}
