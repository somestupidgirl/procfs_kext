#ifndef procfs_internal_h
#define procfs_internal_h

#include <mach/thread_info.h>
#include <sys/file.h>
#include <sys/kernel_types.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>

#pragma mark -
#pragma mark Macro definitions from XNU

// bsd/sys/mount_internal.h
#define VFC_VFS64BITREADY   	0x100
#define VFC_VFSNOMACLABEL   	0x1000

// bsd/sys/proc_internal.h
#define PROC_RETURNED      		(0)
#define PROC_ALLPROCLIST  		(1U << 0)

// osfmk/mach/resource_monitors.h
#define MAXCOMLEN               16

// bsd/sys/malloc.h
#define M_CACHE                 26

#pragma mark -
#pragma mark Type definitions

typedef char command_t[MAXCOMLEN + 1];

#pragma mark -
#pragma mark Structures

struct proc {
    uid_t                   p_uid;
    gid_t                   p_gid;
    struct filedesc        *p_fd;
    command_t               p_comm;
    struct timeval          p_start;                /* starting time */
};

struct vfs_context {
    thread_t        vc_thread;              /* pointer to Mach thread */
    kauth_cred_t    vc_ucred;               /* per thread credential */
};

#pragma mark -
#pragma mark External References

extern task_t proc_task(proc_t);
extern proc_t proc_find(int pid);
extern struct proc *current_proc(void);

// Missing symbols
#if 0
typedef int (*proc_iterate_fn_t)(proc_t, void *);
extern void proc_list_lock(void);
extern void proc_list_unlock(void);
extern void proc_fdlock(struct proc *);
extern void proc_fdlock_spin(struct proc *);
extern void proc_fdunlock(struct proc *);
extern void proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
extern thread_t convert_port_to_thread(ipc_port_t port);
extern kern_return_t task_threads(task_inspect_t target_task, thread_act_array_t *act_list, mach_msg_type_number_t *act_listCnt);
extern kern_return_t thread_info(thread_inspect_t target_act, thread_flavor_t flavor, thread_info_t thread_info_out, mach_msg_type_number_t *thread_info_outCnt);
#endif

#endif /* procfs_internal_h */
