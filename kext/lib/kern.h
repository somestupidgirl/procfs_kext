#ifndef _procfs_kern_h
#define _procfs_kern_h

extern int proc_pidshortbsdinfo(proc_t p, struct proc_bsdshortinfo * pbsd_shortp, int zombie);
extern int proc_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo);
extern int proc_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo);

extern int fill_fileinfo(struct fileproc * fp, proc_t p, vnode_t vp, int vid, int fd, struct proc_fileinfo * fi, vfs_context_t ctx);
extern int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);

#endif