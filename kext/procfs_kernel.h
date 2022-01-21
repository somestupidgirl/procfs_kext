#ifndef procfs_kernel_h
#define procfs_kernel_h

#include <stdatomic.h>

#include <mach/message.h>
#include <mach/task.h>
#include <mach/thread_info.h>
#include <mach/i386/kern_return.h>

#include <os/refcnt.h>

#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/guarded.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/types.h>

#pragma mark -
#pragma mark Function declarations

void fill_fileinfo(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo);

// Symbols not found in any library kext:

extern int nprocs, maxproc;

extern void proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);

extern kern_return_t thread_info(thread_act_t target_thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t thread_info_count);
extern kern_return_t task_threads(task_inspect_t target_task, thread_act_array_t *act_list, mach_msg_type_number_t *act_listCnt);

extern thread_t convert_port_to_thread(mach_port_t);

extern int fp_drop(struct proc *p, int fd, struct fileproc *fp, int locked);
extern int fp_getfvpandvid(proc_t p, int fd, struct fileproc **resultfp, struct vnode **resultvp, uint32_t *vidp);
extern int fp_getfsock(struct proc *p, int fd, struct fileproc **resultfp, struct socket  **results);

extern int fill_socketinfo(socket_t so, struct socket_info *si);
static int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);

#endif /* procfs_kern */
