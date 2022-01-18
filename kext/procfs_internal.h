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

typedef char command_t[MAXCOMLEN + 1];


#pragma mark -
#pragma mark Structures

struct fileproc;
struct socket;
struct socket_info;

struct filedesc {
	struct file           **fd_ofiles;
	char                   *fd_ofileflags;
	int                     fd_nfiles;		/* number of open files allocated */
};

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

#endif /* procfs_internal_h */
