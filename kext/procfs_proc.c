#include <os/base.h>

#include <kern/debug.h>
#include <kern/kalloc.h>

#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/queue.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include "procfs_proc.h"

#pragma mark -
#pragma mark External References

extern struct proc *current_proc(void);
extern void procfs_list_lock(void);
extern void procfs_list_unlock(void);
extern void procfs_pgrp_lock(struct pgrp * pgrp);
extern void procfs_pgrp_unlock(struct pgrp * pgrp);
extern void procfs_tty_lock(struct tty *tp);
extern void procfs_tty_unlock(struct tty *tp);

#pragma mark -
#pragma mark Function Prototypes

void procfs_proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
int procfs_proc_pidbsdinfo(proc_t p, struct proc_bsdinfo * pbsd, int zombie);

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

static inline proc_t
procfs_pfind_locked(pid_t pid)
{
    proc_t p;
    pid = proc_pid(p);
    uint32_t getpid = proc_find(pid);

    if (!pid) {
        return kernproc;
    }

    for (p = PIDHASH(pid)->lh_first; p != 0; p = p->p_hash.le_next) {
        if (getpid == pid) {
            return p;
        }
    }
    return NULL;
}

static inline int
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

static inline proc_t
procfs_proc_find_zombref(int pid)
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

static inline void
procfs_proc_drop_zombref(proc_t p)
{
    procfs_list_lock();
    if ((p->p_listflag & P_LIST_WAITING) == P_LIST_WAITING) {
        p->p_listflag &= ~P_LIST_WAITING;
        wakeup(&p->p_stat);
    }
    procfs_list_unlock();
}

#pragma mark -
#pragma mark Global Functions

void
procfs_proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg)
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

int
procfs_proc_pidbsdinfo(proc_t p, struct proc_bsdinfo * pbsd, int zombie)
{

	struct tty *tp;
	struct session *sessionp = NULL;
	struct pgrp *pg;
    struct proc_fdinfo *fdi;
    struct filedesc *fdp;

    pg = proc_pgrpid(p);
    sessionp = proc_sessionid(p);
    fdp = fdi->proc_fd;

    int pid = proc_pid(p);
    int ppid = proc_ppid(p);

    kauth_cred_t my_cred;
	my_cred = kauth_cred_proc_ref(p);
	bzero(pbsd, sizeof(struct proc_bsdinfo));
	pbsd->pbi_status = p->p_stat;
	pbsd->pbi_xstatus = p->p_xstat;
	pbsd->pbi_pid = proc_find(pid);
	pbsd->pbi_ppid = proc_find(ppid);
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

    int is64bit = proc_is64bit(p);
    int isinexit = proc_exiting(p);

    pbsd->pbi_flags = 0;
	if ((p->p_flag & P_SYSTEM) == P_SYSTEM) {
		pbsd->pbi_flags |= PROC_FLAG_SYSTEM;
	}
	if ((p->p_lflag & P_LTRACED) == P_LTRACED) {
		pbsd->pbi_flags |= PROC_FLAG_TRACED;
	}
	if (isinexit == 0) {
		pbsd->pbi_flags |= PROC_FLAG_INEXIT;
	}
	if ((p->p_lflag & P_LPPWAIT) == P_LPPWAIT) {
		pbsd->pbi_flags |= PROC_FLAG_PPWAIT;
	}
	if (is64bit == 0) {
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
		pbsd->pbi_nfiles = &fdp->fd_nfiles;
	}

	pbsd->e_tdev = NODEV;
	if (pg != PGRP_NULL) {
		pbsd->pbi_pgid = pg;
		pbsd->pbi_pjobc = pg->pg_jobc;
		if ((p->p_flag & P_CONTROLT) && (sessionp != SESSION_NULL) && (tp = SESSION_TP(sessionp))) {
			pbsd->e_tdev = tp->t_dev;
			pbsd->e_tpgid = sessionp->s_ttypgrpid;
		}
	}
#if 0
	if (sessionp != SESSION_NULL) {
		session_rele(sessionp);
	}

	if (pg != PGRP_NULL) {
		pg_rele(pg);
	}
#endif
	return 0;
}
