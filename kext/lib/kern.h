#ifndef _procfs_kern_h
#define _procfs_kern_h

extern int proc_pidshortbsdinfo(proc_t p, struct proc_bsdshortinfo * pbsd_shortp, int zombie);
extern int proc_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo);
extern int proc_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo);

extern void fill_fileinfo(struct fileproc * fp, proc_t proc, int fd, struct proc_fileinfo * fproc);
extern int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, __unused boolean_t check_fsgetpath);
extern int fp_getfvpandvid(proc_t p, int fd, struct fileproc **resultfp, struct vnode **resultvp, uint32_t *vidp);
extern int fp_getfsock(proc_t p, int fd, struct fileproc **resultfp, socket_t *results);
extern int fp_drop(proc_t p, int fd, struct fileproc *fp, int locked);

#endif