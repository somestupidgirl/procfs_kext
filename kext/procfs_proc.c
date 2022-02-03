#include <os/base.h>

#include <kern/debug.h>
#include <kern/kalloc.h>
#include <kern/task.h>

#include <mach/thread_info.h>

#include <sys/bsdtask_info.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/queue.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include "procfs_proc.h"

#pragma mark -
#pragma mark External References

extern vm_map_t kernel_map;
extern vm_map_size_t vm_map_adjusted_size(vm_map_t map);
extern struct proc *current_proc(void);
extern void procfs_list_lock(void);
extern void procfs_list_unlock(void);
extern void procfs_pgrp_lock(struct pgrp * pgrp);
extern void procfs_pgrp_unlock(struct pgrp * pgrp);
extern void procfs_tty_lock(struct tty *tp);
extern void procfs_tty_unlock(struct tty *tp);
extern void fill_taskprocinfo(task_t task, struct proc_taskinfo_internal * ptinfo);
extern int fill_taskthreadinfo(task_t task, uint64_t thaddr, bool thuniqueid, struct proc_threadinfo_internal * ptinfo, void * vpp, int *vidp);

#pragma mark -
#pragma mark Function Prototypes

task_t procfs_task(proc_t proc);
proc_t procfs_find_zombref(int pid);
void procfs_drop_zombref(proc_t p);
void procfs_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
int procfs_pidbsdinfo(proc_t p, struct proc_bsdinfo * pbsd, int zombie);
int procfs_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo);
int procfs_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo);

#pragma mark -
#pragma mark Structures

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
#pragma mark Static Inline Functions

static inline pidlist_t *
procfs_pidlist_init(pidlist_t *pl)
{
	SLIST_INIT(&pl->pl_head);
	pl->pl_active = NULL;
	pl->pl_nalloc = 0;
	return pl;
}

static inline u_int
procfs_pidlist_alloc(pidlist_t *pl, u_int needed)
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

static inline u_int
procfs_pidlist_nalloc(const pidlist_t *pl)
{
	return pl->pl_nalloc;
}

static inline void
procfs_pidlist_set_active(pidlist_t *pl)
{
	pl->pl_active = SLIST_FIRST(&pl->pl_head);
	assert(pl->pl_active);
}

static inline void
procfs_pidlist_free(pidlist_t *pl)
{
    pidlist_entry_t *pe;
    while (NULL != (pe = SLIST_FIRST(&pl->pl_head))) {
        SLIST_FIRST(&pl->pl_head) = SLIST_NEXT(pe, pe_link);
        kalloc(sizeof(*pe));
    }
    pl->pl_nalloc = 0;
}

static inline int
procfs_transwait(proc_t p, int locked)
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
static inline proc_t
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

/*
 * delete a process group
 */
static inline void
procfs_pgdelete_dropref(struct pgrp *pgrp)
{
    struct tty *ttyp;
    int emptypgrp  = 1;
    struct session *sessp;
    lck_grp_t * proc_mlock_grp;

    pgrp = _MALLOC_ZONE((u_int32_t)sizeof(struct pgrp), M_PGRP, M_WAITOK);
    if (!pgrp) {
        panic("procfs_pgdelete_dropref: unable to allocate pgrp structure\n");
    }
    bzero((char *)pgrp, sizeof(struct pgrp));

    sessp = _MALLOC_ZONE((u_int32_t)sizeof(struct session), M_TTYS, M_WAITOK);
    if (!sessp) {
        panic("procfs_pgdelete_dropref: unable to allocate session structure\n");
    }
    bzero((char *)&sessp, sizeof(struct session));

    procfs_pgrp_lock(pgrp);
    if (pgrp->pg_membercnt != 0) {
        emptypgrp = 0;
    }
    procfs_pgrp_unlock(pgrp);

    procfs_list_lock();
    pgrp->pg_refcount--;
    if ((emptypgrp == 0) || (pgrp->pg_membercnt != 0)) {
        procfs_list_unlock();
        return;
    }

    pgrp->pg_listflags |= PGRP_FLAG_TERMINATE;

    if (pgrp->pg_refcount > 0) {
        procfs_list_unlock();
        return;
    }

    pgrp->pg_listflags |= PGRP_FLAG_DEAD;
    LIST_REMOVE(pgrp, pg_hash);

    procfs_list_unlock();

    ttyp = SESSION_TP(pgrp->pg_session);
    if (ttyp != TTY_NULL) {
        if (ttyp->t_pgrp == pgrp) {
            procfs_tty_lock(ttyp);
            /* Re-check after acquiring the lock */
            if (ttyp->t_pgrp == pgrp) {
                ttyp->t_pgrp = NULL;
                pgrp->pg_session->s_ttypgrpid = NO_PID;
            }
            procfs_tty_unlock(ttyp);
        }
    }

    procfs_list_lock();

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
        procfs_list_unlock();

        if (ttyp != TTY_NULL) {
            procfs_tty_lock(ttyp);
            if (ttyp->t_session == sessp) {
                ttyp->t_session = NULL;
            }
            procfs_tty_unlock(ttyp);
        }

        procfs_list_lock();
        sessp->s_listflags |= S_LIST_DEAD;
        if (sessp->s_count != 0) {
            panic("pg_deleteref: freeing session in use");
        }
        procfs_list_unlock();
        lck_mtx_destroy(&sessp->s_mlock, proc_mlock_grp);
        _FREE_ZONE(&sessp, sizeof(struct session), M_TTYS);
    } else {
        procfs_list_unlock();
    }
    lck_mtx_destroy(&pgrp->pg_mlock, proc_mlock_grp);
    _FREE_ZONE(pgrp, sizeof(struct pgrp), M_PGRP);
}

static inline void
procfs_pg_rele_dropref(struct pgrp * pgrp)
{
    procfs_list_lock();
    if ((pgrp->pg_refcount == 1) && ((pgrp->pg_listflags & PGRP_FLAG_TERMINATE) == PGRP_FLAG_TERMINATE)) {
        procfs_list_unlock();
        procfs_pgdelete_dropref(pgrp);
        return;
    }

    pgrp->pg_refcount--;
    procfs_list_unlock();
}

static inline struct pgrp *
procfs_pgrp(proc_t p)
{
    struct pgrp * pgrp;
    lck_mtx_t * proc_list_mlock;

    if (p == PROC_NULL) {
        return PGRP_NULL;
    }
    procfs_list_lock();

    while ((p->p_listflag & P_LIST_PGRPTRANS) == P_LIST_PGRPTRANS) {
        p->p_listflag |= P_LIST_PGRPTRWAIT;
        (void)msleep(&p->p_pgrpid, proc_list_mlock, 0, "procfs_pgrp", 0);
    }

    pgrp = p->p_pgrp;

    assert(pgrp != NULL);

    if (pgrp != PGRP_NULL) {
        pgrp->pg_refcount++;
        if ((pgrp->pg_listflags & (PGRP_FLAG_TERMINATE | PGRP_FLAG_DEAD)) != 0) {
            panic("procfs_pgrp: ref being povided for dead pgrp");
        }
    }

    procfs_list_unlock();

    return pgrp;
}

static inline void
procfs_pg_rele(struct pgrp * pgrp)
{
    if (pgrp == PGRP_NULL) {
        return;
    }
    procfs_pg_rele_dropref(pgrp);
}

static inline void
procfs_session_rele(struct session *sess)
{
    sess = _MALLOC_ZONE((u_int32_t)sizeof(struct session), M_TTYS, M_WAITOK);
    if (!sess) {
        panic("procfs_session_rele: unable to allocate session structure\n");
    }
    bzero((char *)&sess, sizeof(struct session));

    lck_grp_t * proc_mlock_grp;
    procfs_list_lock();
    if (--sess->s_count == 0) {
        if ((sess->s_listflags & (S_LIST_TERM | S_LIST_DEAD)) != 0) {
            panic("procfs_session_rele: terminating already terminated session");
        }
        sess->s_listflags |= S_LIST_TERM;
        LIST_REMOVE(sess, s_hash);
        sess->s_listflags |= S_LIST_DEAD;
        if (sess->s_count != 0) {
            panic("session_rele: freeing session in use");
        }
        procfs_list_unlock();
        lck_mtx_destroy(&sess->s_mlock, proc_mlock_grp);
        _FREE_ZONE(&sess, sizeof(struct session), M_TTYS);
    } else {
        procfs_list_unlock();
    }
}

static inline struct session *
procfs_session(proc_t p)
{
    struct session * sess = SESSION_NULL;
    lck_mtx_t * proc_list_mlock;

    if (p == PROC_NULL) {
        return SESSION_NULL;
    }

    procfs_list_lock();

    /* wait during transitions */
    while ((p->p_listflag & P_LIST_PGRPTRANS) == P_LIST_PGRPTRANS) {
        p->p_listflag |= P_LIST_PGRPTRWAIT;
        (void)msleep(&p->p_pgrpid, proc_list_mlock, 0, "procfs_pgrp", 0);
    }

    if ((p->p_pgrp != PGRP_NULL) && ((sess = p->p_pgrp->pg_session) != SESSION_NULL)) {
        if ((sess->s_listflags & (S_LIST_TERM | S_LIST_DEAD)) != 0) {
            panic("procfs_session:returning sesssion ref on terminating session");
        }
        sess->s_count++;
    }
    procfs_list_unlock();
    return sess;
}

#pragma mark -
#pragma mark Global Functions

proc_t
procfs_find_zombref(int pid)
{
    proc_t p;
    lck_mtx_t * proc_list_mlock;

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
procfs_drop_zombref(proc_t p)
{
    procfs_list_lock();
    if ((p->p_listflag & P_LIST_WAITING) == P_LIST_WAITING) {
        p->p_listflag &= ~P_LIST_WAITING;
        wakeup(&p->p_stat);
    }
    procfs_list_unlock();
}

void
procfs_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg)
{
	uint32_t nprocs = 0;

	pidlist_t pid_list, *pl = procfs_pidlist_init(&pid_list);
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
	const pidlist_entry_t *pe;
    SLIST_FOREACH(pe, &(pl->pl_head), pe_link) {
        for (u_int i = 0; i < pe->pe_nused; i++) {
            const pid_t pid = pe->pe_pid[i];
            proc_t p = proc_find(pid);
            if (p) {
                if ((flags & PROC_NOWAITTRANS) == 0) {
                    procfs_transwait(p, 0);
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
                p = procfs_find_zombref(pid);
                if (!p) {
                    continue;
                }
                const int callout_ret = callout(p, arg);

                switch (callout_ret) {
                case PROC_RETURNED_DONE:
                    procfs_drop_zombref(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED_DONE:
                    goto out;

                case PROC_RETURNED:
                    procfs_drop_zombref(p);
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

int
procfs_pidbsdinfo(proc_t p, struct proc_bsdinfo * pbsd, int zombie)
{

	struct tty *tp;
	struct session *sessionp = NULL;
	struct pgrp * pg;
	kauth_cred_t my_cred;

	pg = procfs_pgrp(p);
	sessionp = procfs_session(p);

	my_cred = kauth_cred_proc_ref(p);
	bzero(pbsd, sizeof(struct proc_bsdinfo));
	pbsd->pbi_status = p->p_stat;
	pbsd->pbi_xstatus = p->p_xstat;
	pbsd->pbi_pid = p->p_pid;
	pbsd->pbi_ppid = p->p_ppid;
	pbsd->pbi_uid = kauth_cred_getuid(my_cred);
	pbsd->pbi_gid = kauth_cred_getgid(my_cred);
	pbsd->pbi_ruid =  kauth_cred_getruid(my_cred);
	pbsd->pbi_rgid = kauth_cred_getrgid(my_cred);
	pbsd->pbi_svuid =  kauth_cred_getsvuid(my_cred);
	pbsd->pbi_svgid = kauth_cred_getsvgid(my_cred);
	kauth_cred_unref(&my_cred);

	pbsd->pbi_nice = p->p_nice;
	pbsd->pbi_start_tvsec = p->p_start.tv_sec;
	pbsd->pbi_start_tvusec = p->p_start.tv_usec;
	bcopy(&p->p_comm, &pbsd->pbi_comm[0], MAXCOMLEN);
	pbsd->pbi_comm[MAXCOMLEN - 1] = '\0';
	bcopy(&p->p_name, &pbsd->pbi_name[0], 2 * MAXCOMLEN);
	pbsd->pbi_name[(2 * MAXCOMLEN) - 1] = '\0';

	pbsd->pbi_flags = 0;
	if ((p->p_flag & P_SYSTEM) == P_SYSTEM) {
		pbsd->pbi_flags |= PROC_FLAG_SYSTEM;
	}
	if ((p->p_lflag & P_LTRACED) == P_LTRACED) {
		pbsd->pbi_flags |= PROC_FLAG_TRACED;
	}
	if ((p->p_lflag & P_LEXIT) == P_LEXIT) {
		pbsd->pbi_flags |= PROC_FLAG_INEXIT;
	}
	if ((p->p_lflag & P_LPPWAIT) == P_LPPWAIT) {
		pbsd->pbi_flags |= PROC_FLAG_PPWAIT;
	}
	if ((p->p_flag & P_LP64) == P_LP64) {
		pbsd->pbi_flags |= PROC_FLAG_LP64;
	}
	if ((p->p_flag & P_CONTROLT) == P_CONTROLT) {
		pbsd->pbi_flags |= PROC_FLAG_CONTROLT;
	}
	if ((p->p_flag & P_THCWD) == P_THCWD) {
		pbsd->pbi_flags |= PROC_FLAG_THCWD;
	}
	if ((p->p_flag & P_SUGID) == P_SUGID) {
		pbsd->pbi_flags |= PROC_FLAG_PSUGID;
	}
	if ((p->p_flag & P_EXEC) == P_EXEC) {
		pbsd->pbi_flags |= PROC_FLAG_EXEC;
	}

	if (sessionp != SESSION_NULL) {
		if (SESS_LEADER(p, sessionp)) {
			pbsd->pbi_flags |= PROC_FLAG_SLEADER;
		}
		if (sessionp->s_ttyvp) {
			pbsd->pbi_flags |= PROC_FLAG_CTTY;
		}
	}

	switch (PROC_CONTROL_STATE(p)) {
	case P_PCTHROTTLE:
		pbsd->pbi_flags |= PROC_FLAG_PC_THROTTLE;
		break;
	case P_PCSUSP:
		pbsd->pbi_flags |= PROC_FLAG_PC_SUSP;
		break;
	case P_PCKILL:
		pbsd->pbi_flags |= PROC_FLAG_PC_KILL;
		break;
	};

	switch (PROC_ACTION_STATE(p)) {
	case P_PCTHROTTLE:
		pbsd->pbi_flags |= PROC_FLAG_PA_THROTTLE;
		break;
	case P_PCSUSP:
		pbsd->pbi_flags |= PROC_FLAG_PA_SUSP;
		break;
	};

#if 0
	/* if process is a zombie skip bg state */
	if ((zombie == 0) && (p->p_stat != SZOMB) && (p->task != TASK_NULL)) {
		proc_get_darwinbgstate(p->task, &pbsd->pbi_flags);
	}
#endif

	if (zombie == 0) {
		pbsd->pbi_nfiles = p->p_fd->fd_nfiles;
	}

	pbsd->e_tdev = NODEV;
	if (pg != PGRP_NULL) {
		pbsd->pbi_pgid = p->p_pgrpid;
		pbsd->pbi_pjobc = pg->pg_jobc;
		if ((p->p_flag & P_CONTROLT) && (sessionp != SESSION_NULL) && (tp = SESSION_TP(sessionp))) {
			pbsd->e_tdev = tp->t_dev;
			pbsd->e_tpgid = sessionp->s_ttypgrpid;
		}
	}

	if (sessionp != SESSION_NULL) {
		procfs_session_rele(sessionp);
	}

	if (pg != PGRP_NULL) {
		procfs_pg_rele(pg);
	}

	return 0;
}

int
procfs_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo)
{
    task_t task;

    task = p->task;

    bzero(ptinfo, sizeof(struct proc_taskinfo));
    fill_taskprocinfo(task, (struct proc_taskinfo_internal *)ptinfo);

    return 0;
}

int
procfs_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo)
{
    int error = 0;
    uint64_t threadaddr = (uint64_t)arg;

    bzero(pthinfo, sizeof(struct proc_threadinfo));

    error = fill_taskthreadinfo(p->task, threadaddr, thuniqueid, (struct proc_threadinfo_internal *)pthinfo, NULL, NULL);
    if (error) {
        return ESRCH;
    } else {
        return 0;
    }
}

task_t
procfs_task(proc_t proc)
{
    return (task_t)proc->task;
}
