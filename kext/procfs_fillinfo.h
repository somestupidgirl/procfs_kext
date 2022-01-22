#ifndef procfs_kernel_h
#define procfs_kernel_h

#pragma mark -
#pragma mark Function declarations

int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);
void fill_fileinfo(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo);
errno_t fill_socketinfo(struct socket *so, struct socket_info *si); // Not yet fully implemented

#endif /* procfs_kern */
