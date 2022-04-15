/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * symdecls.h
 *
 * Declarations for the symbol resolver and related stuff.
 */
#ifndef _symbols_h
#define _symbols_h

#include <i386/cpuid.h>
#include <sys/bsdtask_info.h>
#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/vnode.h>

struct tty;
#define TTY_NULL (struct tty *)NULL

#pragma mark -
#pragma mark Global variables for process.

extern struct proc *            (*_initproc);
#define                         initproc \
                                *_initproc;

extern struct proclist          (*_allproc);
#define                         allproc \
                                *_allproc

extern int                      (*_nprocs);
#define                         nprocs \
                                *_nprocs

extern int                      (*_maxproc);
#define                         maxproc \
                                *_maxproc

extern int                      (*_maxprocperuid);
#define                         maxprocperuid \
                                *_maxprocperuid

extern int                      (*_hard_maxproc);
#define                         hard_maxproc \
                                *_hard_maxproc

#pragma mark -
#pragma mark Process misc functions.

/*
 * Returns the task field in struct proc.
 */
extern task_t                   (*_proc_task)(proc_t proc);
#define                         proc_task(p) \
                                _proc_task(p)
/*
 * Set process start time. Returns 0 on success.
 */
extern int                      (*_proc_starttime)(proc_t p, struct timeval *tv);
#define                         proc_starttime(p, tv)   \
                                _proc_starttime(p, tv)
/*
 * Is the passed-in process tainted by uid or gid changes system call?
 * Returns 1 if tainted, 0 if not tainted.
 */
extern int                      (*_proc_issetugid)(proc_t p);
#define                         proc_issetugid(p) \
                                _proc_issetugid(p)
/*
 * Mutex lock for process.
 */
extern void                     (*_proc_lock)(void);
#define                         proc_list_lock() \
                                _proc_list_lock()
/*
 * Mutex unlock for process.
 */
extern void                     (*_proc_unlock)(void);
#define                         proc_list_unlock() \
                                _proc_list_unlock()
/*
 * Mutex lock for process list.
 */
extern void                     (*_proc_list_lock)(void);
#define                         proc_list_lock() \
                                _proc_list_lock()
/*
 * Mutex unlock for process list.
 */
extern void                     (*_proc_list_unlock)(void);
#define                         proc_list_unlock() \
                                _proc_list_unlock()
/*
 * Process iteration.
 */
extern void                     (*_proc_iterate)(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
#define                         proc_iterate(flags, co, arg, ff, fa) \
                                _proc_iterate(flags, co, arg, ff, fa)

#pragma mark -
#pragma mark Process Group

/*
 * Get process group by passed-in process.
 */
extern struct pgrp *            (*_proc_pgrp)(proc_t);
#define                         proc_pgrp(p) \
                                _proc_pgrp(p)
/*
 * Get process group by passed-in PID.
 */
extern struct pgrp *            (*_pgfind)(pid_t);
#define                         pgfind(pid) \
                                _pgfind(pid)
/*
 * Release process group.
 */
extern void                     (*_pg_rele)(struct pgrp * pgrp);
#define                         pg_rele(pgrp) \
                                _pg_rele(pgrp)
/*
 * Process group mutex lock.
 */
extern void                     (*_pgrp_lock)(struct pgrp * pgrp);
#define                         pgrp_lock(pgrp) \
                                _pgrp_lock(pgrp)
/*
 * Process group mutex unlock.
 */
extern void                     (*_pgrp_unlock)(struct pgrp * pgrp);
#define                         pgrp_unlock(pgrp) \
                                _pgrp_unlock(pgrp)
/*
 * Process group iteration.
 */
extern void                     (*_pgrp_iterate)(struct pgrp *pgrp, unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
#define                         pgrp_iterate(pgrp, flags, co, arg, ff, fa) \
                                _pgrp_iterate(pgrp)

#pragma mark -
#pragma mark Session

/*
 *  Get session id by passed-in process.
 */
extern struct session *         (*_proc_session)(proc_t p);
#define                         proc_session(p) \
                                _proc_session(p)
/*
 * Release session.
 */
extern void                     (*_session_rele)(struct session *sess);
#define                         session_rele(sess) \
                                _session_rele(sess)

extern struct session *         (*_session_find_internal)(pid_t sessid);
#define                         session_find_internal(sess) \
                                _session_find_internal(sess)

/*
 * Create a new session and set the process group ID to the session ID
 * Returns 0 on success, EPERM on failure.
 */
extern int                      (*_setsid)(proc_t p, __unused struct setsid_args *uap, int32_t *retval);
#define                         setsid(p, uap, retval) \
                                _setsid(p, uap, retval)
/*
 * Core implementation of setsid().
 */
extern int                      (*_setsid_internal)(struct proc *p);
#define                         setsid_internal(p) \
                                _setsid_internal(p)
/*
 * Mutex lock for session.
 */
extern void                     (*_session_lock)(struct session * sess);
#define                         session_lock(sess) \
                                _session_lock(sess)
/*
 * Mutex unlock for session.
 */
extern void                     (*_session_unlock)(struct session * sess);
#define                         session_unlock(sess) \
                                _session_unlock(sess)

#pragma mark -
#pragma mark TTY

/*
 * Copies the associated tty vnode for a given process if it exists.
 * The caller needs to decrement the iocount of the vnode.
 * Rreturns 0 on success. ENOENT if the process has no associated TTY.
 * EINVAL if arguments are NULL or vnode_getwithvid fails.
 */
extern int                      (*_proc_gettty)(proc_t p, vnode_t *vp);
#define                         proc_gettty(p, vp) \
                                _proc_gettty(p, vp)
/*
 * Populates the associated tty device for a given process if it exists.
 * Returns 0 on success or EINVAL on failure.
 */
extern int                      (*_proc_gettty_dev)(proc_t p, dev_t *dev);
#define                         proc_gettty_dev(p, dev) \
                                _proc_gettty_dev(p, dev)
/*
 * Mutex lock for TTY.
 */
extern void                     (*_tty_lock)(struct tty *tp);
#define                         tty_lock(tp) \
                                _tty_lock(tp)
/*
 * Mutex unlock for TTY.
 */
extern void                     (*_tty_unlock)(struct tty *tp);
#define                         tty_unlock(tp) \
                                _tty_unlock(tp)

#pragma mark -
#pragma mark File descriptor

/*
 * Copies a filedesc structure.  This is normally used as part of
 * forkproc() when forking a new process, to copy the per process
 * open file table over to the new process.
 */
extern struct filedesc *        (*_fdcopy)(proc_t p, struct vnode *uth_cdir);
#define                         fdcopy(p, uth_cdir) \
                                _fdcopy(p, uth_cdir)
/* 
 * Release a filedesc (per process open file table) structure;
 * this is done on process exit(), or from forkproc_free() if
 * the fork fails for some reason subsequent to a successful
 * call to fdcopy().
 */
extern void                     (*_fdfree)(proc_t p);
#define                         fdfree(p) \
                                _fdfree(p)
/*
 * Mutex lock for process file descriptor.
 */
extern void                     (*_proc_fdlock)(proc_t p);
#define                         proc_fdlock(p) \
                                _proc_fdlock(p)
/*
 * Mutex spinlock for process file descriptor.
 */
extern void                     (*_proc_fdlock_spin)(proc_t p);
#define                         proc_fdlock_spin(p) \
                                _proc_fdlock_spin(p)
/*
 * Mutex unlock for process file descriptor.
 */
extern void                     (*_proc_fdunlock)(proc_t p);
#define                         proc_fdunlock(p) \
                                _proc_fdunlock(p)
/*
 * Filedesc table iteration: next.
 */
extern struct fdt_iterator      (*_fdt_next)(proc_t p, int fd, bool only_settled);
#define                         fdt_next(p, fd, only_settled) \
                                _fdt_next(p, fd, only_settled)
/*
 * Filedesc table iteration: previous.
 */
extern struct fdt_iterator      (*_fdt_prev)(proc_t p, int fd, bool only_settled);
#define                         fdt_prev(p, fd, only_settled) \
                                _fdt_prev(p, fd, only_settled)

#pragma mark -
#pragma mark KPI functions from libproc.

/*
 * Update the darwin background action state in the flags field for libproc.
 */
extern int                      (*_proc_get_darwinbgstate)(task_t task, uint32_t * flagsp);
#define                         proc_get_darwinbgstate(task, flagsp) \
                                _proc_get_darwinbgstate(task, flagsp)
/*
 * Fill the proc_taskinfo_internal structure.
 */
extern int                      (*_fill_taskprocinfo)(task_t task, struct proc_taskinfo_internal * ptinfo);
#define                         fill_taskprocinfo(task, ptinfo) \
                                _fill_taskprocinfo(task, ptinfo)
/*
 * Fill the proc_threadinfo_internal structure.
 */
extern int                      (*_fill_taskthreadinfo)(task_t task, uint64_t thaddr, bool thuniqueid, struct proc_threadinfo_internal * ptinfo, void * vpp, int *vidp);
#define                         fill_taskthreadinfo(task, thaddr, thuniqueid, ptinfo, vpp, vidp) \
                                _fill_taskthreadinfo(task, thaddr, thuniqueid, ptinfo, vpp, vidp)
/*
 * Fill the socket_info structure.
 */
extern int                      (*_fill_socketinfo)(socket_t so, struct socket_info *si);
#define                         fill_socketinfo(so, si) \
                                _fill_socketinfo(so, si)
/*
 * Caller is responsible for adding margin to *count when calling this in
 * circumstances where file descriptors can appear/disappear between the
 * two calls to this function.
 */
extern int                      (*_proc_fdlist)(proc_t p, struct proc_fdinfo *buf, size_t *count);
#define                         proc_fdlist(p, buf, count) \
                                _proc_fdlist(p, buf, count)

#pragma mark -
#pragma mark Threads

/*
 * 
 */
extern kern_return_t            (*_thread_info)(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);
#define                         thread_info(thread, flavor, thread_info, thread_info_count) \
                                _thread_info(thread, flavor, thread_info, thread_info_count)
/*
 * Task threads. Returns KERN_SUCCESS on success.
 */
extern kern_return_t            (*_task_threads)(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);
#define                         task_threads(task, threads_out, count) \
                                _task_threads(task, threads_out, count)
/*
 * Convert port to thread.
 */
extern thread_t                 (*_convert_port_to_thread)(ipc_port_t port);
#define                         convert_port_to_thread(port) \
                                _convert_port_to_thread(port)
/*
 * Returns the Mach thread associated with a vfs_context_t.
 */
extern thread_t                 (*_vfs_context_thread)(vfs_context_t ctx);
#define                         vfs_context_thread(ctx) \
                                _vfs_context_thread(ctx)
/*
 * Returns the uthread field from struct thread.
 */
extern void *                   (*_get_bsdthread_info)(thread_t);
#define                         get_bsdthread_info(t) \
                                _get_bsdthread_info(t)

#pragma mark -
#pragma mark Mount

/*
 * Global variable that tells if a mount point is dead.
 */
extern struct mount *           (*_dead_mountp);
#define                         dead_mountp \
                                *_dead_mountp

#pragma mark -
#pragma mark Vnode

/*
 * Vnode status.
 */
extern int                      (*_vn_stat)(struct vnode *vp, void * sb, kauth_filesec_t *xsec, int isstat64, int needsrealdev, vfs_context_t ctx);
#define                         vn_stat(vp, sb, xsec, isstat64, needsrealdev, ctx) \
                                _vn_stat(vp, sb, xsec, isstat64, needsrealdev, ctx)
/*
 * Mutex lock for vnode.
 */
extern void                     (*_vnode_lock)(struct vnode *);
#define                         vnode_lock(vp) \
                                _vnode_lock(vp)
/*
 * Mutex unlock for vnode.
 */
extern void                     (*_vnode_unlock)(struct vnode *);
#define                         vnode_unlock(vp) \
                                _vnode_unlock(vp)

#pragma mark -
#pragma mark CPU

/*
 * Fills the i386_cpu_info structure and returns its pointer.
 */
extern i386_cpu_info_t *        (*_cpuid_info)(void);
#define                         cpuid_info() \
                                _cpuid_info()
/*
 * Global variable that stores the processor count on the current CPU.
 */
extern unsigned int             (*_processor_count);
#define                         processor_count \
                                *_processor_count
/*
 * Global variable that stores the TSC frequency of the current CPU.
 */
extern uint64_t                 (*_tscFreq);
#define                         tscFreq \
                                *_tscFreq

#endif /* _symbols_h */