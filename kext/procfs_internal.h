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

// bsd/sys/filedesc.h
#define UF_RESERVED        		0x04

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

// osfmk/mach/resource_monitors.h
typedef char command_t[MAXCOMLEN + 1];


#pragma mark -
#pragma mark Structures

struct filedesc {
	struct file           **fd_ofiles;
	char                   *fd_ofileflags;
	int                     fd_nfiles;		/* number of open files allocated */
};

struct proc {
    struct filedesc        *p_fd;
    command_t               p_comm;
    struct timeval          p_start;                /* starting time */
};

struct vfs_context {
    thread_t        vc_thread;              /* pointer to Mach thread */
    kauth_cred_t    vc_ucred;               /* per thread credential */
};

struct fileproc;
struct socket;
struct socket_info;


#pragma mark -
#pragma mark Function Prototypes for procfs_internal.c

// bsd/sys/file_internal.h
// Reimplementations of these two functions since they are no longer available in XNU
// Merely dummy functions for now, until we figure out how to make them kext-compatible.
int fp_getfvpandvid(proc_t p, int fd, struct fileproc **resultfp, struct vnode **resultvp, uint32_t *vidp);
int fp_getfsock(proc_t p, int fd, struct fileproc **resultfp, struct socket **results);


#pragma mark -
#pragma mark External function definitions from XNU

// bsd/sys/file_internal.h
extern int fp_drop(struct proc *p, int fd, struct fileproc *fp, int locked);

// bsd/kern/proc.h
extern task_t proc_task(proc_t);
extern uint32_t proc_getuid(proc_t);
extern uint32_t proc_getgid(proc_t);

// bsd/kern/proc_info.c
extern int proc_pidfdlist(proc_t p, user_addr_t buffer, uint32_t  buffersize, int32_t *retval);
extern int proc_pidbsdinfo(proc_t p, struct proc_bsdinfo *pinfo, int zombie);
extern int proc_pidtaskinfo(proc_t p, struct proc_taskinfo *tinfo);
extern int proc_pidthreadinfo(proc_t p, uint64_t threadid,  int thuniqueid, struct proc_threadinfo *info);
extern void fill_fileinfo(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo);
extern int fill_socketinfo(socket_t so, struct socket_info *si);
static int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);

// bsd/sys/proc_internal.h
extern int nprocs, maxproc;
extern struct proc *current_proc(void);
extern struct proclist allproc;
typedef int (*proc_iterate_fn_t)(proc_t, void *);
extern void proc_list_lock(void);
extern void proc_list_unlock(void);
extern void proc_fdlock(proc_t *);
extern void proc_fdlock_spin(proc_t *);
extern void proc_fdunlock(proc_t *);
extern void session_lock(struct session * sess);
extern void session_unlock(struct session * sess);
extern void proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);

#endif /* procfs_internal_h */
