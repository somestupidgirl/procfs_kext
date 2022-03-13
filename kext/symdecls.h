#ifndef _symdecls_h
#define _symdecls_h

#include <i386/cpuid.h>
#include <sys/filedesc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>

struct proc;
struct pgrp;
struct session;
struct fileproc;
struct vnode_attr;
struct tty;
struct proc_threadinfo_internal;
struct proc_taskinfo_internal;

#pragma mark -
#pragma mark Type Definitions

typedef int (*proc_iterate_fn_t)(proc_t, void *);

#pragma mark -
#pragma mark Global Variables

extern struct proc * (*_initproc);
extern struct proclist (*_allproc);

extern int (*_nprocs);
extern int (*_maxproc);
extern int (*_maxprocperuid);
extern int (*_hard_maxproc);
extern unsigned int (*_processor_count);
extern uint64_t (*_tscFreq);

#pragma mark -
#pragma mark Function Declarations

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

extern struct pgrp * (*_proc_pgrp)(proc_t);
extern struct pgrp * (*_tty_pgrp)(struct tty * tp);
extern struct session * (*_proc_session)(proc_t p);
extern void (*_pgrp_iterate)(struct pgrp *pgrp, unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
extern void (*_proc_iterate)(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
extern int (*_proc_starttime)(proc_t p, struct timeval *tv);
extern task_t (*_proc_task)(proc_t proc);
extern int (*_proc_issetugid)(proc_t p);
extern void (*_proc_name)(int pid, char * buf, int size);
extern int (*_proc_gettty)(proc_t p, vnode_t *vp);
extern int (*_proc_gettty_dev)(proc_t p, dev_t *dev);
extern int (*_proc_get_darwinbgstate)(task_t task, uint32_t * flagsp);
extern void (*_proc_reparentlocked)(struct proc *child, struct proc * newparent, int cansignal, int locked);
extern int (*_proc_fdlist)(proc_t p, struct proc_fdinfo *buf, size_t *count);
extern struct fdt_iterator (*_fdt_next)(proc_t p, int fd, bool only_settled);

extern int (*_fill_socketinfo)(socket_t so, struct socket_info *si);
extern int (*_fill_taskprocinfo)(task_t task, struct proc_taskinfo_internal * ptinfo);
extern int (*_fill_taskthreadinfo)(task_t task, uint64_t thaddr, bool thuniqueid, struct proc_threadinfo_internal * ptinfo, void * vpp, int *vidp);
extern thread_t (*_convert_port_to_thread)(ipc_port_t port);
extern kern_return_t (*_task_threads)(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);
extern kern_return_t (*_thread_info)(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);

extern int (*_vn_stat)(struct vnode *vp, void * sb, kauth_filesec_t *xsec, int isstat64, int needsrealdev, vfs_context_t ctx);

extern int (*_cpu_number)(void);
extern i386_cpu_info_t *(*_cpuid_info)(void);
extern uint64_t (*_cpuid_features)(void);
extern uint64_t (*_cpuid_extfeatures)(void);
extern uint64_t (*_cpuid_leaf7_features)(void);
extern uint64_t (*_cpuid_leaf7_extfeatures)(void);
extern char * (*_cpuid_get_feature_names)(uint64_t, char *, unsigned);
extern char * (*_cpuid_get_extfeature_names)(uint64_t, char *, unsigned);
extern char * (*_cpuid_get_leaf7_feature_names)(uint64_t, char *, unsigned);
extern char * (*_cpuid_get_leaf7_extfeature_names)(uint64_t, char *, unsigned);

#pragma mark -
#pragma mark Macros

#define _fdt_foreach(fp, p) \
    for (struct fdt_iterator __fdt_it = _fdt_next(p, -1, true); \
        ((fp) = __fdt_it.fdti_fp); \
        __fdt_it = _fdt_next(p, __fdt_it.fdti_fd, true))

#endif /* _symdecls_h */