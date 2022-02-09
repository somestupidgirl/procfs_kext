#include <kern/kern_types.h>
#include <mach/mach_types.h>

#include <sys/cdefs.h>
#include <sys/proc.h>
#include <sys/types.h>

#include "kernel_resolver.h"

#pragma mark -
#pragma mark Local Definitions

typedef int kern_return_t;
typedef int (*proc_iterate_fn_t)(proc_t, void *);

#pragma mark -
#pragma mark Function Prototypes

void                PROC_LIST_LOCK(void);
void                PROC_LIST_UNLOCK(void);
void                PROC_FDLOCK_SPIN(proc_t p);
void                PROC_FDUNLOCK(proc_t p);
int                 PROC_PIDBSDINFO(proc_t p, struct proc_bsdinfo * pbsd, int zombie);
int                 PROC_PIDTASKINFO(proc_t p, struct proc_taskinfo * ptinfo);
int                 PROC_PIDTHREADINFO(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo);
int                 PROC_GETTTY(proc_t p, vnode_t *vp);
struct pgrp *       PROC_PGRP(proc_t p);
kern_return_t       PROC_ITERATE(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
task_t              PROC_TASK(proc_t p);
int                 FILL_VNODEINFO(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);
void                FILL_FILEINFO(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo);
errno_t             FILL_SOCKETINFO(struct socket *so, struct socket_info *si);
kern_return_t       TASK_THREADS(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);
kern_return_t       THREAD_INFO(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);
thread_t            CONVERT_PORT_TO_THREAD(ipc_port_t port);

#pragma mark -
#pragma mark Symbol Resolver

void            (*_proc_list_lock)(void);
void            (*_proc_list_unlock)(void);
void            (*_proc_fdlock_spin)(proc_t p);
void            (*_proc_fdunlock)(proc_t p);
int             (*_proc_pidbsdinfo)(proc_t p, struct proc_bsdinfo * pbsd, int zombie);
int             (*_proc_pidtaskinfo)(proc_t p, struct proc_taskinfo * ptinfo);
int             (*_proc_pidthreadinfo)(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo);
int             (*_proc_gettty)(proc_t p, vnode_t *vp);
struct pgrp *   (*_proc_pgrp)(proc_t p);
kern_return_t   (*_proc_iterate)(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
task_t          (*_proc_task)(proc_t);
int             (*_fill_vnodeinfo)(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);
void            (*_fill_fileinfo)(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo);
errno_t         (*_fill_socketinfo)(struct socket *so, struct socket_info *si);
kern_return_t   (*_task_threads)(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);
kern_return_t   (*_thread_info)(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);
thread_t        (*_convert_port_to_thread)(ipc_port_t port);

#pragma mark -
#pragma mark Functions

void
PROC_LIST_LOCK(void)
{
    _proc_list_lock = (void*)lookup_symbol("_proc_list_lock");

    return _proc_list_lock()
}

void
PROC_LIST_UNLOCK(void)
{
    _proc_list_unlock = (void*)lookup_symbol("_proc_list_unlock");

    return _proc_list_unlock()
}

void
PROC_FDLOCK_SPIN(proc_t p)
{
    _proc_fdlock_spin = (void*)lookup_symbol("_proc_fdlock_spin");

    return _proc_fdlock_spin(p);
}

void
PROC_FDUNLOCK(proc_t p)
{
    _proc_fdunlock = (void*)lookup_symbol("_proc_fdunlock");

    return _proc_fdunlock(p);
}

int
PROC_PIDBSDINFO(proc_t p, struct proc_bsdinfo * pbsd, int zombie)
{
    _proc_pidbsdinfo = (void*)lookup_symbol("_proc_pidbsdinfo");

    return _proc_pidbsdinfo(p, &pbsd, zombie);
}

int
PROC_PIDTASKINFO(proc_t p, struct proc_taskinfo * ptinfo)
{
    _proc_pidtaskinfo = (void*)lookup_symbol("_proc_pidtaskinfo");

    return _proc_pidtaskinfo(p, &ptinfo);
}

int
PROC_PIDTHREADINFO(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo)
{
    _proc_pidthreadinfo = (void*)lookup_symbol("_proc_pidthreadinfo");

    return _proc_pidthreadinfo(p, arg, thuniqueid, &pthinfo);
}

int
PROC_GETTTY(proc_t p, vnode_t *vp)
{
    _proc_getty = (void*)lookup_symbol("_proc_getty");

    return _proc_getty(p, &vp);
}

struct pgrp *
PROC_PGRP(proc_t p)
{
    _proc_pgrp = (void*)lookup_symbol("_proc_pgrp");

    return _proc_pgrp(p);
}

kern_return_t
PROC_ITERATE(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg)
{
    _proc_iterate = (void*)lookup_symbol("_proc_iterate");

    return  _proc_iterate(flags, callout, &arg, filterfn, &filterarg);
}

task_t
PROC_TASK(proc_t p)
{
    _proc_task = (void*)lookup_symbol("_proc_task");

    return _proc_task(p);
}

int
FILL_VNODEINFO(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath)
{
    _fill_vnodeinfo = (void*)lookup_symbol("_fill_vnodeinfo");

    return _fill_vnodeinfo(vp, &vinfo, check_fsgetpath);
}

void
FILL_FILEINFO(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo)
{
    _fill_fileinfo = (void*)lookup_symbol("_fill_fileinfo");

    return _fill_fileinfo(&fp, proc, fd, &finfo);
}

errno_t
FILL_SOCKETINFO(struct socket *so, struct socket_info *si)
{
    _fill_socketinfo = (void*)lookup_symbol("_fill_socketinfo");

    return _fill_socketinfo(&so, &si);
}

kern_return_t
TASK_THREADS(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count)
{
    _task_threads = (void*)lookup_symbol("_task_threads");

    return _task_threads(task, &threads_out, &count);
}

kern_return_t
THREAD_INFO(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count)
{
    _thread_info = (void*)lookup_symbol("_thread_info");

    return _thread_info(thread, flavor, thread_info, &thread_info_count);
}

thread_t
CONVERT_PORT_TO_THREAD(ipc_port_t port)
{
    _convert_port_to_thread = (void*)lookup_symbol("_convert_port_to_thread");

    return _convert_port_to_thread(port);
}
