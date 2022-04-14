/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * symdecls.h
 *
 * Declarations for the symbol resolver and related stuff.
 */
#ifndef _symdecls_h
#define _symdecls_h

#include <i386/cpuid.h>
#include <sys/bsdtask_info.h>
#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/vnode.h>

#include "kern.h"

#pragma mark -
#pragma mark Global Variables

extern struct proc *        (*_initproc);
extern struct proclist      (*_allproc);

extern int                  (*_nprocs);
extern int                  (*_maxproc);
extern int                  (*_maxprocperuid);
extern int                  (*_hard_maxproc);
extern unsigned int         (*_processor_count);
extern uint64_t             (*_tscFreq);
extern struct mount *       (*_dead_mountp);

#pragma mark -
#pragma mark Functions

extern void                 (*_vnode_lock)(struct vnode *);
extern void                 (*_vnode_unlock)(struct vnode *);
extern void                 (*_proc_list_lock)(void);
extern void                 (*_proc_list_unlock)(void);
extern void                 (*_proc_fdlock)(struct proc *);
extern void                 (*_proc_fdlock_spin)(struct proc *);
extern void                 (*_proc_fdunlock)(struct proc *);

extern struct pgrp *        (*_proc_pgrp)(proc_t);
extern struct session *     (*_proc_session)(proc_t p);
extern void                 (*_proc_iterate)(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
extern int                  (*_proc_starttime)(proc_t p, struct timeval *tv);
extern task_t               (*_proc_task)(proc_t proc);
extern int                  (*_proc_issetugid)(proc_t p);
extern int                  (*_proc_gettty)(proc_t p, vnode_t *vp);
extern int                  (*_proc_gettty_dev)(proc_t p, dev_t *dev);
extern int                  (*_proc_get_darwinbgstate)(task_t task, uint32_t * flagsp);
extern rlim_t               (*_proc_limitgetcur)(proc_t p, int which, boolean_t to_lock_proc);
extern int                  (*_proc_fdlist)(proc_t p, struct proc_fdinfo *buf, size_t *count);
extern struct fdt_iterator  (*_fdt_next)(proc_t p, int fd, bool only_settled);
extern struct filedesc *    (*_fdcopy)(proc_t p, struct vnode *uth_cdir);
extern void                 (*_fdfree)(proc_t p);
extern int                  (*_fill_socketinfo)(socket_t so, struct socket_info *si);
extern int                  (*_fill_taskprocinfo)(task_t task, struct proc_taskinfo_internal * ptinfo);
extern int                  (*_fill_taskthreadinfo)(task_t task, uint64_t thaddr, bool thuniqueid, struct proc_threadinfo_internal * ptinfo, void * vpp, int *vidp);
extern void                 (*_bsd_threadcdir)(void * uth, void *vptr, int *vidp);
extern void *               (*_get_bsdthread_info)(thread_t);
extern thread_t             (*_convert_port_to_thread)(ipc_port_t port);
extern kern_return_t        (*_task_threads)(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);
extern kern_return_t        (*_thread_info)(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);
extern thread_t             (*_vfs_context_thread)(vfs_context_t ctx);
extern int                  (*_vn_stat)(struct vnode *vp, void * sb, kauth_filesec_t *xsec, int isstat64, int needsrealdev, vfs_context_t ctx);
extern i386_cpu_info_t *    (*_cpuid_info)(void);

/*
 * Remove the underscore. It's only needed if we're including
 * headers from MacKernelSDK containing the same function even
 * though it's a private KPI function, rendering it useless.
 * Prefacing the functions with an underscore prevents linker
 * errors in such scenarios.
 */
#define initproc                                            *_initproc
#define allproc                                             *_allproc
#define nprocs                                              *_nprocs
#define maxproc                                             *_maxproc
#define maxprocperuid                                       *_maxprocperuid
#define hard_maxproc                                        *_hard_maxproc
#define processor_count                                     *_processor_count
#define tscFreq                                             *_tscFreq
#define dead_mountp                                         *_dead_mountp

#define vnode_lock(vp)                                      _vnode_lock(vp)
#define vnode_unlock(vp)                                    _vnode_unlock(vp)
#define proc_list_lock()                                    _proc_list_lock()
#define proc_list_unlock()                                  _proc_list_unlock()
#define proc_fdlock(p)                                      _proc_fdlock(p)
#define proc_fdlock_spin(p)                                 _proc_fdlock_spin(p)
#define proc_fdunlock(p)                                    _proc_fdunlock(p)

#define proc_pgrp(p)                                        _proc_pgrp(p)
#define proc_session(p)                                     _proc_session(p)
#define proc_iterate(f, c, a, ff, fa)                       _proc_iterate(f, c, a, ff, fa)
#define proc_starttime(p, tv)                               _proc_starttime(p, tv)
#define proc_task(p)                                        _proc_task(p)
#define proc_issetugid(p)                                   _proc_issetugid(p)
#define proc_gettty(p, vp)                                  _proc_gettty(p, vp)
#define proc_gettty_dev(p, dev)                             _proc_gettty_dev(p, dev)
#define proc_get_darwinbgstate(t, fp)                       _proc_get_darwinbgstate(t, fp)
#define proc_limitgetcur(p, w, tlp)                         _proc_limitgetcur(p, w, tlp)
#define proc_fdlist(p, b, c)                                _proc_fdlist(p, b, c)
#define fdt_next(p, fd, os)                                 _fdt_next(p, fd, os)
#define fdcopy(p, uc)                                       _fdcopy(p, uc)
#define fdfree(p)                                           _fdfree(p)
#define fill_socketinfo(so, si)                             _fill_socketinfo(so, si)
#define fill_taskprocinfo(t, pti)                           _fill_taskprocinfo(t, pti)
#define fill_taskthreadinfo(t, ta, tuid, pti, vpp, vidp)    _fill_taskthreadinfo(t, ta, tuid, pti, vpp, vidp)
#define bsd_threadcdir(uth, vptr, vidp)                     _bsd_threadcdir(uth, vptr, vidp)
#define get_bsdthread_info(t)                               _get_bsdthread_info(t)
#define convert_port_to_thread(port)                        _convert_port_to_thread(port)
#define vfs_context_thread(ctx)                             _vfs_context_thread(ctx)
#define vn_stat(vp, sb, xsec, iss, nrd, ctx)                _vn_stat(vp, sb, xsec, iss, nrd, ctx)
#define cpuid_info()                                        _cpuid_info()
#define task_threads(t, to, c)                              _task_threads(t, to, c)
#define thread_info(t, f, ti, tic)                          _thread_info(t, f, ti, tic)

#endif /* _symdecls_h */