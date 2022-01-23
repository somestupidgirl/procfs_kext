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

typedef struct procfs_pidlist_entry {
    SLIST_ENTRY(pidlist_entry) pe_link;
    u_int pe_nused;
    pid_t pe_pid[PIDS_PER_ENTRY];
} procfs_pidlist_entry_t;

typedef struct {
    SLIST_HEAD(, procfs_pidlist_entry) pl_head;
    struct procfs_pidlist_entry *pl_active;
    u_int pl_nalloc;
} procfs_pidlist_t;


#pragma mark -
#pragma mark External References

extern struct proc *current_proc(void);


#pragma mark -
#pragma mark Static Inline Functions

static __inline__ procfs_pidlist_t *
procfs_pidlist_init(procfs_pidlist_t *pl)
{
	SLIST_INIT(&pl->pl_head);
	pl->pl_active = NULL;
	pl->pl_nalloc = 0;
	return pl;
}

static __inline__ u_int
procfs_pidlist_alloc(procfs_pidlist_t *pl, u_int needed)
{
	while (pl->pl_nalloc < needed) {
		procfs_pidlist_entry_t *pe = kalloc(sizeof(*pe));
		if (NULL == pe) {
			panic("no space for pidlist entry");
		}
		SLIST_INSERT_HEAD(&pl->pl_head, pe, pe_link);
		pl->pl_nalloc += (sizeof(pe->pe_pid) / sizeof(pe->pe_pid[0]));
	}
	return pl->pl_nalloc;
}

static __inline__ u_int
procfs_pidlist_nalloc(const procfs_pidlist_t *pl)
{
	return pl->pl_nalloc;
}

static __inline__ void
procfs_pidlist_set_active(procfs_pidlist_t *pl)
{
	pl->pl_active = SLIST_FIRST(&pl->pl_head);
	assert(pl->pl_active);
}

static __inline__ void
procfs_pidlist_free(procfs_pidlist_t *pl)
{
    procfs_pidlist_entry_t *pe;
    while (NULL != (pe = SLIST_FIRST(&pl->pl_head))) {
        SLIST_FIRST(&pl->pl_head) = SLIST_NEXT(pe, pe_link);
        kalloc(sizeof(*pe));
    }
    pl->pl_nalloc = 0;
}

static __inline__ int
procfs_proc_transwait(proc_t p, int locked)
{
	if (locked == 0) {
		procfs_list_lock();
	}
	while ((p->p_lflag & P_LINTRANSIT) == P_LINTRANSIT) {
		if ((p->p_lflag & P_LTRANSCOMMIT) == P_LTRANSCOMMIT && current_proc() == p) {
			if (locked == 0) {
				procfs_list_unlock();
			}
			return EDEADLK;
		}
		p->p_lflag |= P_LTRANSWAIT;
		msleep(&p->p_lflag, &p->p_mlock, 0, "proc_signstart", NULL);
	}
	if (locked == 0) {
		procfs_list_unlock();
	}
	return 0;
}

/*
 * Locate a process by number
 */
static __inline__ proc_t
procfs_pfind_locked(pid_t pid)
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
procfs_proc_find_zombref(int pid)
{
    proc_t p;

    procfs_list_lock();

again:
    p = procfs_pfind_locked(pid);

    /* should we bail? */
    if ((p == PROC_NULL)                                    /* not found */
        || ((p->p_listflag & P_LIST_INCREATE) != 0)         /* not created yet */
        || ((p->p_listflag & P_LIST_EXITED) == 0)) {        /* not started exit */
        procfs_list_unlock();
        return PROC_NULL;
    }

    /* If someone else is controlling the (unreaped) zombie - wait */
    if ((p->p_listflag & P_LIST_WAITING) != 0) {
        lck_mtx_t *proc_list_mlock;
        (void)msleep(&p->p_stat, proc_list_mlock, PWAIT, "waitcoll", 0);
        goto again;
    }
    p->p_listflag |=  P_LIST_WAITING;

    procfs_list_unlock();

    return p;
}

void
procfs_proc_drop_zombref(proc_t p)
{
    procfs_list_lock();
    if ((p->p_listflag & P_LIST_WAITING) == P_LIST_WAITING) {
        p->p_listflag &= ~P_LIST_WAITING;
        wakeup(&p->p_stat);
    }
    procfs_list_unlock();
}

void
procfs_session_rele(struct session *sess)
{
    procfs_list_lock();
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
        procfs_list_unlock();
        lck_mtx_destroy(&sess->s_mlock, proc_mlock_grp);
        //zfree(session_zone, sess);
    } else {
        procfs_list_unlock();
    }
}

void
procfs_proc_iterate(unsigned int flags, procfs_proc_iterate_fn_t callout, void *arg, procfs_proc_iterate_fn_t filterfn, void *filterarg)
{
	uint32_t nprocs = 0;

	procfs_pidlist_t pid_list, *pl = procfs_pidlist_init(&pid_list);
	u_int pid_count_available = 0;

	assert(callout != NULL);

	/* allocate outside of the procfs_list_lock */
	for (;;) {
		procfs_list_lock();
		pid_count_available = nprocs + 1; /* kernel_task not counted in nprocs */
		assert(pid_count_available > 0);
		if (procfs_pidlist_nalloc(pl) > pid_count_available) {
			break;
		}
		procfs_list_unlock();

		procfs_pidlist_alloc(pl, pid_count_available);
	}
	procfs_pidlist_set_active(pl);

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
	procfs_list_unlock();

	/* call callout on processes in the pid_list */
	const procfs_pidlist_entry_t *pe;
    SLIST_FOREACH(pe, &(pl->pl_head), pe_link) {
        for (u_int i = 0; i < pe->pe_nused; i++) {
            const pid_t pid = pe->pe_pid[i];
            proc_t p = proc_find(pid);
            if (p) {
                if ((flags & PROC_NOWAITTRANS) == 0) {
                    procfs_proc_transwait(p, 0);
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
                p = procfs_proc_find_zombref(pid);
                if (!p) {
                    continue;
                }
                const int callout_ret = callout(p, arg);

                switch (callout_ret) {
                case PROC_RETURNED_DONE:
                    procfs_proc_drop_zombref(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED_DONE:
                    goto out;

                case PROC_RETURNED:
                    procfs_proc_drop_zombref(p);
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
    procfs_pidlist_free(pl);
}
