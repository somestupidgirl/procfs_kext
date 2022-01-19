#ifndef procfs_kernel_h
#define procfs_kernel_h

// Symbols not found in any library kext:

extern kern_return_t thread_info(thread_act_t target_thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t thread_info_count);
extern kern_return_t task_threads(task_inspect_t target_task, thread_act_array_t *act_list, mach_msg_type_number_t *act_listCnt);
extern thread_t convert_port_to_thread( ipc_port_t port);

extern int fp_drop(struct proc *p, int fd, struct fileproc *fp, int locked);
extern int fp_getfvpandvid(proc_t p, int fd, struct fileproc **resultfp, struct vnode **resultvp, uint32_t *vidp);
struct socket;
extern int fp_getfsock(struct proc *p, int fd, struct fileproc **resultfp, struct socket  **results);

extern void fill_fileinfo(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo);
extern int fill_socketinfo(socket_t so, struct socket_info *si);
static int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);

#endif /* procfs_kern */
