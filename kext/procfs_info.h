#ifndef _procfs_info_h
#define _procfs_info_h

extern int procfs_proc_pidshortbsdinfo(proc_t p, struct proc_bsdshortinfo * pbsd_shortp, int zombie);
extern int procfs_proc_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo);
extern int procfs_proc_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo);
extern int procfs_fill_vnodeinfo(vnode_t vp, struct vnode_info *vi, vfs_context_t ctx);
extern void procfs_fill_fileinfo(struct fileproc * fp, proc_t proc, int fd, struct proc_fileinfo * fproc);

#endif /* _procfs_info_h */