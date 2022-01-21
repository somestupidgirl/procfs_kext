#ifndef procfs_kernel_h
#define procfs_kernel_h

#include <stdatomic.h>

#include <mach/message.h>
#include <mach/task.h>
#include <mach/thread_info.h>
#include <mach/i386/kern_return.h>

#include <os/refcnt.h>

#include <sys/errno.h>
#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/guarded.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/types.h>


#pragma mark
#pragma mark Definitions

// osfmk/mach/mach_types.h
typedef unsigned int 			mach_thread_flavor_t;
#define THREAD_FLAVOR_CONTROL   0

// osfmk/kern/ipc_tt.h
__options_decl(port_to_thread_options_t, uint32_t, {
	PORT_TO_THREAD_NONE               = 0x0000,
	PORT_TO_THREAD_IN_CURRENT_TASK    = 0x0001,
	PORT_TO_THREAD_NOT_CURRENT_THREAD = 0x0002,
});


#pragma mark
#pragma mark External References

extern task_t proc_task(proc_t);
extern proc_t proc_find(int pid);
extern struct proc *current_proc(void);


#pragma mark -
#pragma mark Function declarations

int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);
void fill_fileinfo(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo);

#pragma mark -
#pragma mark Not yet implemented

// Not yet implemented
kern_return_t thread_info(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);
kern_return_t task_threads(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);
thread_t convert_port_to_thread(mach_port_t);
errno_t fill_socketinfo(struct socket *so, struct socket_info *si);
void proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);


#endif /* procfs_kern */
