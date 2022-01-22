#ifndef procfs_iterate_h
#define procfs_iterate_h

// We're not ready to migrate away from relying on sys/proc_internal.h yet.
// TODO: Remove the guards once sys/proc_internal.h is no longer needed.
#if 0
#define PROC_NULL (struct proc *)NULL

LIST_HEAD(proclist, proc);
struct proclist allproc;
struct proclist zombproc;

/* process iteration */
#define ALLPROC_FOREACH(var) \
    LIST_FOREACH((var), &allproc, p_list)

#define ZOMBPROC_FOREACH(var) \
    LIST_FOREACH((var), &zombproc, p_list)

/*
 * proc_iterate walks the `allproc` and/or `zombproc` lists, calling `filterfn`
 * with `filterarg` for each process.  For processes where `filterfn` returned
 * non-zero, `callout` is called with `arg`.  If the `PROC_NOWAITTRANS` flag is
 * set, this function waits for transitions.
 *
 * `ALLPROC_FOREACH` or `ZOMBPROC_FOREACH` might also be used under the
 * `procfs_list_lock` to achieve a similar effect.
 */
#define PROC_ALLPROCLIST    (1U << 0)       /* walk the allproc list (processes not yet exited) */
#define PROC_ZOMBPROCLIST   (1U << 1)       /* walk the zombie list */
#define PROC_NOWAITTRANS    (1U << 2)       /* do not wait for transitions (checkdirs only) */

/*
 * These are the only valid return values of `callout` functions provided to
 * process iterators.
 *
 * CLAIMED returns expect the caller to call proc_rele on the proc.  DONE
 * returns stop iterating processes early.
 */
#define PROC_RETURNED       (0)
#define PROC_RETURNED_DONE  (1)
#define PROC_CLAIMED        (2)
#define PROC_CLAIMED_DONE   (3)

/* p_listflag */
#define P_LIST_WAITING      0x00000010
#define P_LIST_EXITED       0x00000040
#define P_LIST_INCREATE     0x00002000

/* local flags */
#define P_LTRANSCOMMIT      0x00000020      /* process is committed to trans */
#define P_LTRANSWAIT        0x00000080      /* waiting for trans to complete */
#endif

typedef int (*procfs_proc_iterate_fn_t)(proc_t, void *);

#pragma mark -
#pragma mark Function Declarations

void procfs_proc_iterate(unsigned int flags, procfs_proc_iterate_fn_t callout, void *arg, procfs_proc_iterate_fn_t filterfn, void *filterarg);

#endif /* procfs_iterate_h */