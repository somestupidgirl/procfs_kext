#include <mach/kern_return.h>

#include "procfs_symbols.h"

// Global Variables
SYM_INIT(nprocs);
SYM_INIT(maxproc);
SYM_INIT(maxprocperuid);
SYM_INIT(hard_maxproc);

// Process Locks
SYM_INIT(proc_lock);
SYM_INIT(proc_unlock);
SYM_INIT(proc_spinlock);
SYM_INIT(proc_spinunlock);
SYM_INIT(proc_list_lock);
SYM_INIT(proc_list_unlock);
SYM_INIT(proc_fdlock);
SYM_INIT(proc_fdlock_spin);
SYM_INIT(proc_fdunlock);
SYM_INIT(proc_fdlock_assert);

// Iteration
SYM_INIT(proc_iterate);
SYM_INIT(pgrp_iterate);

// Process Group and Session
SYM_INIT(proc_pgrp);
SYM_INIT(tty_pgrp);
SYM_INIT(proc_session);

// UID/GID
SYM_INIT(proc_issetugid);

// Process Name
SYM_INIT(proc_name);

// Process TTY
SYM_INIT(proc_gettty);
SYM_INIT(proc_gettty_dev);

// Process Start Time
SYM_INIT(proc_starttime);

// Process Task
SYM_INIT(proc_task);

// Proces Info
SYM_INIT(fill_socketinfo);
SYM_INIT(fill_taskprocinfo);
SYM_INIT(fill_taskthreadinfo);

// Process Misc
SYM_INIT(proc_get_darwinbgstate);

// File Descriptor Table
SYM_INIT(fdt_next);

// Thread
SYM_INIT(convert_port_to_thread);
SYM_INIT(task_threads);
SYM_INIT(thread_info);

// Vnode
SYM_INIT(vn_stat);

// Vnode Operations
SYM_INIT(VNOP_LOOKUP);
SYM_INIT(VNOP_CREATE);
SYM_INIT(VNOP_WHITEOUT);
SYM_INIT(VNOP_MKNOD);
SYM_INIT(VNOP_OPEN);
SYM_INIT(VNOP_CLOSE);
SYM_INIT(VNOP_ACCESS);
SYM_INIT(VNOP_GETATTR);
SYM_INIT(VNOP_SETATTR);
SYM_INIT(VNOP_SELECT);
SYM_INIT(VNOP_EXCHANGE);
SYM_INIT(VNOP_REVOKE);
SYM_INIT(VNOP_MMAP_CHECK);
SYM_INIT(VNOP_MMAP);
SYM_INIT(VNOP_MNOMAP);
SYM_INIT(VNOP_REMOVE);
SYM_INIT(VNOP_LINK);
SYM_INIT(VNOP_RENAME);
SYM_INIT(VNOP_RENAMEX);
SYM_INIT(VNOP_MKDIR);
SYM_INIT(VNOP_RMDIR);
SYM_INIT(VNOP_SYMLINK);
SYM_INIT(VNOP_READDIR);
SYM_INIT(VNOP_READDIRATTR);
SYM_INIT(VNOP_GETATTRLISTBULK);
SYM_INIT(VNOP_READLINK);
SYM_INIT(VNOP_INACTIVE);
SYM_INIT(VNOP_RECLAIM);
SYM_INIT(VNOP_PATHCONF);
SYM_INIT(VNOP_ADVLOCK);
SYM_INIT(VNOP_ALLOCATE);
SYM_INIT(VNOP_PAGEIN);
SYM_INIT(VNOP_PAGEOUT);
SYM_INIT(VNOP_SEARCHFS);
SYM_INIT(VNOP_COPYFILE);
SYM_INIT(VNOP_CLONEFILE);

kern_return_t
resolve_symbols()
{
	// Global Variables
	SYM_LOOKUP(nprocs);
	SYM_LOOKUP(maxproc);
	SYM_LOOKUP(maxprocperuid);
	SYM_LOOKUP(hard_maxproc);

	// Process Locks
	SYM_LOOKUP(proc_lock);
	SYM_LOOKUP(proc_unlock);
	SYM_LOOKUP(proc_spinlock);
	SYM_LOOKUP(proc_spinunlock);
	SYM_LOOKUP(proc_list_lock);
	SYM_LOOKUP(proc_list_unlock);
	SYM_LOOKUP(proc_fdlock);
	SYM_LOOKUP(proc_fdlock_spin);
	SYM_LOOKUP(proc_fdunlock);
	SYM_LOOKUP(proc_fdlock_assert);

	// Process Iteration
	SYM_LOOKUP(proc_iterate);
	SYM_LOOKUP(pgrp_iterate);

	// Process Group and Session
	SYM_LOOKUP(proc_pgrp);
	SYM_LOOKUP(tty_pgrp);
	SYM_LOOKUP(proc_session);

	// UID/GID
	SYM_LOOKUP(proc_issetugid);

	// Process Name
	SYM_LOOKUP(proc_name);

	// Process TTY
	SYM_LOOKUP(proc_gettty);
	SYM_LOOKUP(proc_gettty_dev);

	// Process Start Time
	SYM_LOOKUP(proc_starttime);

	// Process Task
	SYM_LOOKUP(proc_task);

	// Proces Info
	SYM_LOOKUP(fill_socketinfo);
	SYM_LOOKUP(fill_taskprocinfo);
	SYM_LOOKUP(fill_taskthreadinfo);

	// Process Misc
	SYM_LOOKUP(proc_get_darwinbgstate);

	// File Descriptor
	SYM_LOOKUP(fdt_next);

	// Thread
	SYM_LOOKUP(convert_port_to_thread);
	SYM_LOOKUP(task_threads);
	SYM_LOOKUP(thread_info);

	// Vnode
	SYM_LOOKUP(vn_stat);

	// Vnode Operations
	SYM_LOOKUP(VNOP_LOOKUP);
	SYM_LOOKUP(VNOP_CREATE);
	SYM_LOOKUP(VNOP_WHITEOUT);
	SYM_LOOKUP(VNOP_MKNOD);
	SYM_LOOKUP(VNOP_OPEN);
	SYM_LOOKUP(VNOP_CLOSE);
	SYM_LOOKUP(VNOP_ACCESS);
	SYM_LOOKUP(VNOP_GETATTR);
	SYM_LOOKUP(VNOP_SETATTR);
	SYM_LOOKUP(VNOP_SELECT);
	SYM_LOOKUP(VNOP_EXCHANGE);
	SYM_LOOKUP(VNOP_REVOKE);
	SYM_LOOKUP(VNOP_MMAP_CHECK);
	SYM_LOOKUP(VNOP_MMAP);
	SYM_LOOKUP(VNOP_MNOMAP);
	SYM_LOOKUP(VNOP_REMOVE);
	SYM_LOOKUP(VNOP_LINK);
	SYM_LOOKUP(VNOP_RENAME);
	SYM_LOOKUP(VNOP_RENAMEX);
	SYM_LOOKUP(VNOP_MKDIR);
	SYM_LOOKUP(VNOP_RMDIR);
	SYM_LOOKUP(VNOP_SYMLINK);
	SYM_LOOKUP(VNOP_READDIR);
	SYM_LOOKUP(VNOP_READDIRATTR);
	SYM_LOOKUP(VNOP_GETATTRLISTBULK);
	SYM_LOOKUP(VNOP_READLINK);
	SYM_LOOKUP(VNOP_INACTIVE);
	SYM_LOOKUP(VNOP_RECLAIM);
	SYM_LOOKUP(VNOP_PATHCONF);
	SYM_LOOKUP(VNOP_ADVLOCK);
	SYM_LOOKUP(VNOP_ALLOCATE);
	SYM_LOOKUP(VNOP_PAGEIN);
	SYM_LOOKUP(VNOP_PAGEOUT);
	SYM_LOOKUP(VNOP_SEARCHFS);
	SYM_LOOKUP(VNOP_COPYFILE);
	SYM_LOOKUP(VNOP_CLONEFILE);
				   
	return KERN_SUCCESS;
}
