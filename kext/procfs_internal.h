#ifndef procfs_internal_h
#define procfs_internal_h

#include <sys/kernel_types.h>
#include <sys/mount.h>


#pragma mark -
#pragma mark Additional kernel types

struct file_type;
typedef struct file_type * file_type_t;

struct filedesc;
typedef struct filedesc * filedesc_t;

struct fileinfo;
typedef struct fileinfo * fileinfo_t;

struct fileops;
typedef struct fileops * fileops_t;

struct fileproc;
typedef struct fileproc * fileproc_t;

struct os_refcnt;
typedef struct os_refcnt * os_refcnt_t;

struct pgrp;
typedef struct pgrp * pgrp_t;

struct proc_bsdinfo;
typedef struct proc_bsdinfo * proc_bsdinfo_t;

struct proc_fileinfo;
typedef struct proc_fileinfo * proc_fileinfo_t;

struct proc_taskinfo;
typedef struct proc_taskinfo * proc_taskinfo_t;

struct proc_threadinfo;
typedef struct proc_threadinfo * proc_threadinfo_t;

struct session;
typedef struct session * session_t;

struct socket;
typedef struct socket * socket_t;

struct socket_fdinfo;
typedef struct socket_fdinfo * socket_fdinfo_t;

struct socket_info;
typedef struct socket_info * socket_info_t;

struct vnode_fdinfowithpath;
typedef struct vnode_fdinfowithpath * vnode_fdinfowithpath_t;

struct vnode_info;
typedef struct vnode_info * vnode_info_t;

struct vnode_info_path;
typedef struct vnode_info_path * vnode_info_path_t;


#pragma mark -
#pragma mark Macro definitions from XNU

// bsd/sys/filedesc.h
#define UF_RESERVED        0x04

// bsd/sys/mount_internal.h
#define VFC_VFS64BITREADY   0x100
#define VFC_VFSNOMACLABEL   0x1000


#pragma mark -
#pragma mark Function Prototypes for procfs_internal.c

// bsd/sys/proc.h
// Reimplementation of proc_task() since the function is not available in
// MacKernelSDK's sys/proc.h due to being guarded by KERNEL_PRIVATE 
task_t procfs_proc_task(proc_t);

// bsd/sys/file_internal.h
// Reimplementations of these two functions since they are no longer available in XNU 
int procfs_fp_getfvpandvid(proc_t *p, int fd, struct fileproc **resultfp, vnode_t **resultvp, uint32_t * vidp);
int procfs_fp_getfsock(struct proc *p, int fd, struct fileproc **resultfp, struct socket **results);


#pragma mark -
#pragma mark TODO FIX: Function definitions from XNU (low priority)

int fp_drop(struct proc *p, int fd, struct fileproc *fp, int locked);

extern int proc_pidbsdinfo(proc_t p, struct proc_bsdinfo *pinfo, int zombie);
extern int proc_pidtaskinfo(proc_t p, struct proc_taskinfo *tinfo);
extern int proc_pidthreadinfo(proc_t p, uint64_t threadid,  int thuniqueid, struct proc_threadinfo *info);

extern void fill_fileinfo(fileproc_t * fp, proc_t proc, int fd, proc_fileinfo_t * finfo);
extern int fill_socketinfo(socket_t so, struct socket_info *si);
static int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);

extern void proc_list_lock(void);
extern void proc_list_unlock(void);

extern void proc_fdlock(proc_t *);
extern void proc_fdlock_spin(proc_t *);
extern void proc_fdunlock(proc_t *);

extern void session_lock(session_t * sess);
extern void session_unlock(session_t * sess);
extern struct session * pgrp_session(struct pgrp * pgrp);

#endif /* procfs_internal_h */
