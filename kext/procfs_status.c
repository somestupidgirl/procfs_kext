/*
 * Copyright (c) 2015 Kim Topley
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_status.c
 *
 * Data and size functions for the per-process and per-thread status nodes (pid, ppid, pgid, sid, tty, info, taskinfo).
 */
#include <libkern/libkern.h>
#include <sys/bsdtask_info.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/kpi_socket.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/vnode.h>
#include <netinet/in.h>

#include <miscfs/procfs/procfs.h>

#include "lib/symbols.h"
#include "lib/kern.h"

/*
 * Reads the data for the "pid" node. The data is the
 * owning process's pid.
 */
int
procfs_read_pid_data(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int pid = pnp->node_id.nodeid_pid;
    int error = procfs_copy_data((const char *)&pid, sizeof(pid), uio);
    return error;
}

/*
 * Reads the data for the "ppid" node. The data is the
 * owning process's parent's pid.
 */
int
procfs_read_ppid_data(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != PROC_NULL) {
        int ppid = proc_ppid(p);
        error = procfs_copy_data((const char *)&ppid, sizeof(ppid), uio);
        proc_rele(p);
    } else {
        error = ESRCH;
    }

    return error;
}

/*
 * Reads the data for the "pgid" node. The data is the
 * owning process's process group id.
 */
int
procfs_read_pgid_data(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != PROC_NULL) {
        pid_t pgrpid = proc_pgrpid(p);
        error = procfs_copy_data((const char *)&pgrpid, sizeof(pgrpid), uio);
        proc_rele(p);
    } else {
        error = ESRCH;
    }

    return error;
}

/*
 * Reads the data for the "sid" node. The data is the
 * owning process's session id.
 */
int
procfs_read_sid_data(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != PROC_NULL) {
        pid_t sid = proc_sessionid(p);
        error = procfs_copy_data((const char *)&sid, sizeof(sid), uio);
        proc_rele(p);
    } else {
        error = ESRCH;
    }

    return error;
}

/*
 * Reads the data for the "tty" node. The data is the
 * name of the owning process's controlling terminal.
 */
int
procfs_read_tty_data(__unused pfsnode_t *pnp, __unused uio_t uio, __unused vfs_context_t ctx)
{
    // The controlling terminal lives at p->p_pgrp->pg_session->s_ttyvp, but
    // p_pgrp is an SMR-protected pointer and every safe way to dereference it
    // bottoms out in symbols a third-party kext cannot link: the SMR read
    // section (smr_enter/smr_leave + the smr_system global) and the slow path
    // (proc_list_lock) are private/unexported, as are proc_gettty(), proc_pgrp()
    // and vnode_hold()/vnode_drop(). Unlike the fd table (a direct struct member
    // we forward-ported), there is no safe forward-port for this. So the tty
    // node reports "unavailable" rather than risking a use-after-free.
    return ENOTSUP;
}

/*
 * Reads basic info for a process. Populates an instance of the proc_bsdinfo
 * structure and copies it to the area described by a uio structure.
 */
int
procfs_dostatus(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    // Get the process id from the node id in the pfsnode and locate
    // the process.
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != PROC_NULL) {
        struct proc_bsdshortinfo info; // Mac OS X >= 10.7 has proc_bsdshortinfo

        // Get the BSD-centric process info and copy it out.
        error = proc_pidshortbsdinfo(p, &info, 0);
        if (error == 0) {
            error = procfs_copy_data((const char *)&info, sizeof(info), uio);
        } else {
            error = ESRCH;
        }
        proc_rele(p);
    }

    return error;
}

/*
 * Reads basic info for the Mach task associated with a process. Populates 
 * an instance of a proc_taskinfo structure and copies it to tthe area described
 * by a uio structure.
 */
int
procfs_read_task_info(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    // Get the process id from the node id in the pfsnode and locate
    // the process.
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != PROC_NULL) {
        struct proc_taskinfo info;

        // Get the task info and copy it out.
        error = proc_pidtaskinfo(p, &info);
        if (error == 0) {
            error = procfs_copy_data((const char *)&info, sizeof(info), uio);
        } else {
            error = ESRCH;
        }
        proc_rele(p);
    }

    return error;
}

/*
 * Reads basic info for a thread. Populates an instance of a proc_threadinfo 
 * structure and copies it to tthe area described by a uio structure.
 */
int
procfs_read_thread_info(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    // Get the process id and thread from the node id in the pfsnode and locate
    // the process.
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != PROC_NULL) {
        struct proc_threadinfo info;
        uint64_t threadid = pnp->node_id.nodeid_objectid;
        
        // Get the task info and copy it out.
        error = proc_pidthreadinfo(p, threadid, TRUE, &info);
        if (error == 0) {
            error = procfs_copy_data((const char *)&info, sizeof(info), uio);
        } else {
            error = ESRCH;
        }
        proc_rele(p);
    }

    return error;
}

/*
 * Gets the value of the st_size field of a node's attributes. POSIX
 * allows us to use this value as we choose. This function calculates 
 * the appropriate size for a node by calling that node's psn_getsize()
 * function. For directories, the size reported by all non-directory
 * child nodes is aggregated to get the result.
 */
size_t
procfs_get_node_size_attr(pfsnode_t *pnp, kauth_cred_t creds)
{
    pfssnode_t *snode = pnp->node_structure_node;
    pfstype node_type = snode->psn_node_type;

    // In the special cases of "." and "..", we need to first move up
    // to the parent and grandparent structure node to get the correct result.
    if (node_type == PFSdirthis) {
        snode = snode->psn_parent;
    } else if (node_type == PFSdirparent) {
        snode = snode->psn_parent;
        if (snode != NULL && snode->psn_node_type != PFSroot) {
            snode = snode->psn_parent;
        }
    }

    assert(snode != NULL);

    // For file types, get the size from the node itself. For
    // directory types, get the size by traversing child structure
    // nodes and adding in any implied children, such as process and
    // thread entries.
    size_t size = 0;
    if (procfs_is_directory_type(node_type)) {
        // Directory
        pfssnode_t *next_snode;
        TAILQ_FOREACH(next_snode, &snode->psn_children, psn_next) {
            procfs_node_size_fn node_size_fn = next_snode->psn_getsize_fn;
            size += node_size_fn == NULL ? 1 : node_size_fn(pnp, creds);
        }
    } else {
        // File or symlink
        procfs_node_size_fn node_size_fn = snode->psn_getsize_fn;
        size = node_size_fn == NULL ? snode->psn_node_size : node_size_fn(pnp, creds);
    }
    
    return size;
}

/*
 * Gets the size for a node that represents a process.
 */
size_t
procfs_process_node_size(pfsnode_t *pnp, kauth_cred_t creds)
{
    // Nodes of this type contribute a size of 1 for each visible process.
    size_t size = 0;
    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
    if (p != PROC_NULL) {
        if (size == 0) {
            size += sizeof(procfs_get_process_count(creds));
        }
        proc_rele(p);
    }

    return size;
}

/*
 * Gets the size for a node that represents a thread.
 */
size_t
procfs_thread_node_size(pfsnode_t *pnp, __unused kauth_cred_t creds)
{
    // Nodes of this type contribute a size of 1 for each thread
    // in the owning process. Because of the way the file system is
    // structured, the pid of the owning process is available in the
    // node_id of the procfs node.
    size_t size = 0;
    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
    if (p != PROC_NULL) {
        if (_proc_task != NULL) {
            task_t task = proc_task(p);
            if (task != NULL) {
                size += sizeof(procfs_get_task_thread_count(task));
            }
        } else {
            size += 1;
        }
        proc_rele(p);
    }
    return size;
}
