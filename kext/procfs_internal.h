#ifndef procfs_internal_h
#define procfs_internal_h

struct proc;
struct pgrp;
struct session;

#pragma mark -
#pragma mark Process NULL Pointers

#define PROC_NULL               (struct proc *)NULL
#define PGRP_NULL               (struct pgrp *)NULL
#define SESSION_NULL            (struct session *)NULL

#pragma mark -
#pragma mark PID Definitions

/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 */
//extern int nprocs, maxproc;
//extern int maxprocperuid;
//extern int hard_maxproc;

#define PID_MAX                 99999
#define NO_PID                  100000

//#define SESS_LEADER(p, sessp)   ((sessp)->s_leader == (p))

//#define PIDHASH(pid)    (&pidhashtbl[(pid) & pidhash])
//extern LIST_HEAD(pidhashhead, proc) * pidhashtbl;
//extern u_long pidhash;

//#define PGRPHASH(pgid)  (&pgrphashtbl[(pgid) & pgrphash])
//extern LIST_HEAD(pgrphashhead, pgrp) * pgrphashtbl;
//extern u_long pgrphash;

//#define SESSHASH(sessid) (&sesshashtbl[(sessid) & sesshash])
//extern LIST_HEAD(sesshashhead, session) * sesshashtbl;
//extern u_long sesshash;

//LIST_HEAD(proclist, proc);
//extern struct struct proclist *p_allproc;
//extern struct proclist *p_zombproc;

#pragma mark -
#pragma mark Process Iteration

typedef int (*proc_iterate_fn_t)(proc_t, void *);

/*
 * These are the only valid return values of `callout` functions provided to
 * process iterators.
 *
 * CLAIMED returns expect the caller to call proc_rele on the proc.  DONE
 * returns stop iterating processes early.
 */
#define PROC_RETURNED           (0)
#define PROC_RETURNED_DONE      (1)
#define PROC_CLAIMED            (2)
#define PROC_CLAIMED_DONE       (3)

/*
 * pgrp_iterate walks the provided process group, calling `filterfn` with
 * `filterarg` for each process.  For processes where `filterfn` returned
 * non-zero, `callout` is called with `arg`.  If `PGRP_DROPREF` is supplied in
 * `flags`, a reference will be dropped from the process group after obtaining
 * the list of processes to call `callout` on.
 *
 * `PGMEMBERS_FOREACH` might also be used under the pgrp_lock to achieve a
 * similar effect.
 */
#define PGRP_DROPREF (1)

//extern void pgrp_iterate(struct pgrp *pgrp, unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);

/*
 * proc_iterate walks the `allproc` and/or `zombproc` lists, calling `filterfn`
 * with `filterarg` for each process.  For processes where `filterfn` returned
 * non-zero, `callout` is called with `arg`.  If the `PROC_NOWAITTRANS` flag is
 * set, this function waits for transitions.
 *
 * `ALLPROC_FOREACH` or `ZOMBPROC_FOREACH` might also be used under the
 * `proc_list_lock` to achieve a similar effect.
 */
#define PROC_ALLPROCLIST        (1U << 0) /* walk the allproc list (processes not yet exited) */
#define PROC_ZOMBPROCLIST       (1U << 1) /* walk the zombie list */
#define PROC_NOWAITTRANS        (1U << 2) /* do not wait for transitions (checkdirs only) */

//extern void proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);

#endif /* procfs_internal_h */