#ifndef procfs_internal_h
#define procfs_internal_h

#include <sys/filedesc.h>

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
extern int (*_nprocs);
extern int (*_maxproc);
extern int (*_maxprocperuid);
extern int (*_hard_maxproc);

#define PID_MAX                 99999
#define NO_PID                  100000

#pragma mark -
#pragma mark Process Locks

extern void (*_proc_lock)(struct proc *);
extern void (*_proc_unlock)(struct proc *);
extern void (*_proc_spinlock)(struct proc *);
extern void (*_proc_spinunlock)(struct proc *);
extern void (*_proc_list_lock)(void);
extern void (*_proc_list_unlock)(void);
extern void (*_proc_fdlock)(struct proc *);
extern void (*_proc_fdlock_spin)(struct proc *);
extern void (*_proc_fdunlock)(struct proc *);
extern void (*_proc_fdlock_assert)(proc_t p, int assertflags);

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

#pragma mark -
#pragma mark Process Group Iteration

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

extern void (*_pgrp_iterate)(struct pgrp *pgrp, unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);

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

extern void (*_proc_iterate)(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);

#pragma mark -
#pragma mark File Descriptor Table Iteration (sys/filedesc.h)

extern struct fdt_iterator (*_fdt_next)(proc_t p, int fd, bool only_settled);

#define _fdt_foreach(fp, p) \
    for (struct fdt_iterator __fdt_it = _fdt_next(p, -1, true); \
        ((fp) = __fdt_it.fdti_fp); \
        __fdt_it = _fdt_next(p, __fdt_it.fdti_fd, true))

#pragma mark -
#pragma mark Private KPI Symbols (sys/proc_internal.h)

extern struct pgrp * (*_proc_pgrp)(proc_t);
extern struct pgrp * (*_tty_pgrp)(struct tty * tp);
extern struct session * (*_proc_session)(proc_t p);

#pragma mark -
#pragma mark Private KPI Symbols (sys/proc.h)

extern int (*_proc_starttime)(proc_t p, struct timeval *tv);
extern task_t (*_proc_task)(proc_t proc);
extern uint32_t (*_proc_getuid)(proc_t);
extern uint32_t (*_proc_getgid)(proc_t);

#pragma mark -
#pragma mark Missing Public KPI Symbols (sys/proc.h)

/* returns the 32-byte name if it exists, otherwise returns the 16-byte name */
extern char * (*_proc_best_name)(proc_t p);

/*!
 *  @function proc_gettty
 *  @abstract Copies the associated tty vnode for a given process if it exists. The caller needs to decrement the iocount of the vnode.
 *  @return 0 on success. ENOENT if the process has no associated TTY. EINVAL if arguments are NULL or vnode_getwithvid fails.
 */
extern int (*_proc_gettty)(proc_t p, vnode_t *vp);

/* this routine populates the associated tty device for a given process if it exists, returns 0 on success or else returns EINVAL */
extern int (*_proc_gettty_dev)(proc_t p, dev_t *dev);

#pragma mark -
#pragma mark Private KPI Symbols (kern/ipc_tt.h)

/* Convert from a port to a thread */
extern thread_t (*_convert_port_to_thread)(ipc_port_t port);

#pragma mark -
#pragma mark Missing Public KPI Symbols (mach/task.h)

/* Routine task_threads */
extern kern_return_t (*_task_threads)(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);

#pragma mark -
#pragma mark Missing Public KPI Symbols (mach/thread_act.h)

/* Routine thread_info */
extern kern_return_t (*_thread_info)(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);

#endif /* procfs_internal_h */
