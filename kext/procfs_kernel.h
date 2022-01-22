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


#pragma mark -
#pragma mark Function declarations

int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, boolean_t check_fsgetpath);
void fill_fileinfo(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo * finfo);


#pragma mark -
#pragma mark Not yet implemented

// Not yet implemented
thread_t convert_port_to_thread(mach_port_t);
errno_t fill_socketinfo(struct socket *so, struct socket_info *si);


#endif /* procfs_kern */
