#ifndef procfs_internal_h
#define procfs_internal_h

#include <mach/thread_info.h>
#include <sys/kernel_types.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/proc.h>
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


#pragma mark -
#pragma mark Structures

// bsd/sys/proc_info.h

struct proc_bsdinfo {
	uint32_t                pbi_flags;              /* 64bit; emulated etc */
	uint32_t                pbi_status;
	uint32_t                pbi_xstatus;
	uint32_t                pbi_pid;
	uint32_t                pbi_ppid;
	uid_t                   pbi_uid;
	gid_t                   pbi_gid;
	uid_t                   pbi_ruid;
	gid_t                   pbi_rgid;
	uid_t                   pbi_svuid;
	gid_t                   pbi_svgid;
	uint32_t                rfu_1;                  /* reserved */
	char                    pbi_comm[MAXCOMLEN];
	char                    pbi_name[2 * MAXCOMLEN];  /* empty if no name is registered */
	uint32_t                pbi_nfiles;
	uint32_t                pbi_pgid;
	uint32_t                pbi_pjobc;
	uint32_t                e_tdev;                 /* controlling tty dev */
	uint32_t                e_tpgid;                /* tty process group id */
	int32_t                 pbi_nice;
	uint64_t                pbi_start_tvsec;
	uint64_t                pbi_start_tvusec;
};

// bsd/sys/proc_info.h
struct proc_taskinfo {
	uint64_t                pti_virtual_size;       /* virtual memory size (bytes) */
	uint64_t                pti_resident_size;      /* resident memory size (bytes) */
	uint64_t                pti_total_user;         /* total time */
	uint64_t                pti_total_system;
	uint64_t                pti_threads_user;       /* existing threads only */
	uint64_t                pti_threads_system;
	int32_t                 pti_policy;             /* default policy for new threads */
	int32_t                 pti_faults;             /* number of page faults */
	int32_t                 pti_pageins;            /* number of actual pageins */
	int32_t                 pti_cow_faults;         /* number of copy-on-write faults */
	int32_t                 pti_messages_sent;      /* number of messages sent */
	int32_t                 pti_messages_received;  /* number of messages received */
	int32_t                 pti_syscalls_mach;      /* number of mach system calls */
	int32_t                 pti_syscalls_unix;      /* number of unix system calls */
	int32_t                 pti_csw;                /* number of context switches */
	int32_t                 pti_threadnum;          /* number of threads in the task */
	int32_t                 pti_numrunning;         /* number of running threads */
	int32_t                 pti_priority;           /* task priority*/
};

// bsd/sys/proc_info.h
struct proc_threadinfo {
	uint64_t                pth_user_time;          /* user run time */
	uint64_t                pth_system_time;        /* system run time */
	int32_t                 pth_cpu_usage;          /* scaled cpu usage percentage */
	int32_t                 pth_policy;             /* scheduling policy in effect */
	int32_t                 pth_run_state;          /* run state (see below) */
	int32_t                 pth_flags;              /* various flags (see below) */
	int32_t                 pth_sleep_time;         /* number of seconds that thread */
	int32_t                 pth_curpri;             /* cur priority*/
	int32_t                 pth_priority;           /*  priority*/
	int32_t                 pth_maxpriority;        /* max priority*/
	char                    pth_name[MAXTHREADNAMESIZE];    /* thread name, if any */
};

// bsd/sys/proc_info.h
struct proc_fdinfo {
	int32_t                 proc_fd;
	uint32_t                proc_fdtype;
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

// bsd/kern/proc_info.c
extern size_t proc_fdlist_internal(proc_t p, struct proc_fdinfo *pfd, size_t numfds);
extern int proc_pidbsdinfo(proc_t p, struct proc_bsdinfo *pinfo, int zombie);
extern int proc_pidtaskinfo(proc_t p, struct proc_taskinfo *tinfo);
extern int proc_pidthreadinfo(proc_t p, uint64_t threadid,  int thuniqueid, struct proc_threadinfo *info);
extern void fill_fileinfo(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo);
extern int fill_socketinfo(socket_t so, struct socket_info *si);
static int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);

// bsd/sys/proc_internal.h
extern int nprocs, maxproc;
extern void proc_list_lock(void);
extern void proc_list_unlock(void);
extern void proc_fdlock(proc_t *);
extern void proc_fdlock_spin(proc_t *);
extern void proc_fdunlock(proc_t *);
extern void session_lock(struct session * sess);
extern void session_unlock(struct session * sess);

#endif /* procfs_internal_h */
