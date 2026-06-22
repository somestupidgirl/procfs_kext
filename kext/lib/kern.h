#ifndef _procfs_kern_h
#define _procfs_kern_h

extern void compute_averunnable(void *);
extern struct loadavg averunnable;

extern int proc_pidshortbsdinfo(proc_t p, struct proc_bsdshortinfo * pbsd_shortp, int zombie);
extern int proc_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo);
extern int proc_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo);

extern int fill_fileinfo(struct fileproc * fp, proc_t p, int fd, struct proc_fileinfo * fi);
extern int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);

/*
 * fp_getfvp() is exported KPI but the SDK's <sys/file.h> omits its declaration
 * (it lives in xnu's bsd/sys/file.h). Declare it here so callers link the real
 * kernel symbol.
 */
extern int fp_getfvp(struct proc *p, int fd, struct fileproc **resultfp, struct vnode **resultvp);

/*
 * Releases the fileproc iocount taken by fp_getfvp() on a target process. Must
 * be used instead of the public file_drop(), which only operates on
 * current_proc() and panics for another process's descriptor.
 */
extern void procfs_fp_drop(proc_t p, struct fileproc *fp);

#endif