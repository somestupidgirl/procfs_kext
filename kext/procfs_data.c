/*
 * Copyright (c) 2015 Kim Topley
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * procfs_data.c
 *
 * The functions in this file provide the data for files in the procfs
 * file system and the size values for all procfs nodes, as seen in the
 * st_size field of the stat structure. Both the data and the size depend
 * on the node type. The correct function for each node is specified in
 * its pfssnode_t.
 */
#include <libkern/libkern.h>
#include <sys/bsdtask_info.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <miscfs/procfs/procfs.h>

#include "lib/symbols.h"
#include "lib/kern.h"

#pragma mark -
#pragma mark Process and Thread Node Data

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
procfs_read_tty_data(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != PROC_NULL) {
        // Get the controlling terminal vnode from the process session,
        // if it has one.
        struct session *sp = proc_session(p);
        if (sp != SESSION_NULL) {
            session_lock(sp);
            vnode_t cttyvp = sp->s_ttyvp;
            session_unlock(sp);
            if (cttyvp != NULLVP) {
                // Convert the vnode to a full path.
                int name_len = MAXPATHLEN;
                char name_buf[MAXPATHLEN + 1];
                vn_getpath(cttyvp, name_buf, &name_len);
                error = procfs_copy_data((const char *)&name_buf, name_len, uio);
            } else {
                error = ENOTTY;
            }
            session_rele(sp);
        }
        proc_rele(p);
    } else {
        error = ESRCH;
    }

    return error;
}

/*
 * Reads basic info for a process. Populates an instance of the proc_bsdinfo
 * structure and copies it to the area described by a uio structure.
 */
int
procfs_read_proc_info(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
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

#pragma mark -
#pragma mark File Node Data

/*
 * Reads the data associated with a file descriptor node. The data is 
 * a vnode_infowithpath structure containing information about both the target
 * vnode and the file itself.
 */
int
procfs_read_fd_data(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx)
{
    int error = 0;

    // We need the file descriptor and the process id. We get
    // both of them from the node id.
    int fd = pnp->node_id.nodeid_objectid;
    proc_t p = proc_find(pnp->node_id.nodeid_pid);

    if (p != PROC_NULL) {
        struct vnode *vp;
        struct fileproc *fp;

        error = fp_getfvp(p, fd, &fp, &vp);
        if (error == 0 && fp != FILEPROC_NULL) {
            // Get a hold on the vnode and check that it did not
            // change id.
            uint32_t vid = vnode_vid(vp);
            error = vnode_getwithvid(vp, vid);
            if (error == 0) {
                // Got the vnode. Pack vnode and file info into
                // a vnode_fdinfowithpath structure.
                struct vnode_fdinfowithpath info;

                bzero(&info, sizeof(info));
                fill_fileinfo(fp, p, vp, vid, fd, &info.pfi, ctx);

                error = fill_vnodeinfo(vp, &info.pvip.vip_vi, FALSE);
                if (error == 0) {
                    // If all is well, add in the file path and copy the data
                    // out to user space.
                    if (error == 0) {
                        int count = MAXPATHLEN;
                        vn_getpath(vp, info.pvip.vip_path, &count);
                        info.pvip.vip_path[MAXPATHLEN-1] = 0;
                        error = procfs_copy_data((const char *)&info, sizeof(info), uio);
                    }
                }
            }
            vnode_put(vp);
            file_drop(fd);
        } else {
            error = EBADF;
        }
        proc_rele(p);
    } else {
        error = ESRCH;
    }

    return error;
}

/*
 * Reads the data associated with a file descriptor that refers to a socket.
 */
int
procfs_read_socket_data(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx)
{
    int error = 0;

    // We need the file descriptor and the process id. We get
    // both of them from the node id.
    int fd = pnp->node_id.nodeid_objectid;
    proc_t p = proc_find(pnp->node_id.nodeid_pid);

    if (p != PROC_NULL) {
        struct fileproc *fp;
        socket_t so;
        vnode_t vp;

        error = fp_getfvp(p, fd, &fp, &vp);
        if (error == 0 && fp != FILEPROC_NULL) {
            // Get the socket and fileproc structures for the file. If the
            // file is not a socket, this fails and we will return an error.
            // Otherwise, the fileproc has an additional iocount, which we
            // must remember to release.
            error = file_socket(fd, &so);
            if (error == 0) {
                uint32_t vid = vnode_vid(vp);
                struct socket_fdinfo info;

                bzero(&info, sizeof(info));
                fill_fileinfo(fp, p, vp, vid, fd, &info.pfi, ctx);

                error = fill_socketinfo(so, &info.psi);
                if (error == 0) {
                    error = procfs_copy_data((const char *)&info, sizeof(info), uio);
                }
            }
            vnode_put(vp);
            file_drop(fd);
        } else {
            error = EBADF;
        }
        proc_rele(p);
    } else {
        error = ESRCH;
    }

    return error;
}

#pragma mark -
#pragma mark Node Data Size

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
        task_t task = proc_task(p);
        if (task != NULL) {
            size += sizeof(procfs_get_task_thread_count(task));
        }
        proc_rele(p);
    }
    return size;
}

/*
 * Gets the size for the node that represents the file descriptors 
 * of a process. Counts one for every open file in the process.
 */
size_t
procfs_fd_node_size(pfsnode_t *pnp, __unused kauth_cred_t creds)
{
    size_t count = 0;
    struct fileproc *fp;

    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);

    if (p == PROC_NULL) {
        count = 0;
    } else {
        proc_fdlock_spin(p);
        fdt_foreach(fp, p) {
            count++;
        }
        proc_fdunlock(p);
        proc_rele(p);
    }

    return count;
}

#pragma mark -
#pragma mark Helper Functions

/*
 * Copies data from the local buffer "data" into the area described
 * by a uio structure. The first byte of "data" is assumed to 
 * correspond to a zero offset, so if the uio structure has its
 * uio_offset set to N, the first byte of data that will be copied
 * is at data[N].
 */
int
procfs_copy_data(const char *data, int data_len, uio_t uio)
{
    int error = 0;

    off_t start_offset = uio_offset(uio);
    data_len -= start_offset;

    if ((data_len >= 0) && (data != NULL)) {
        error = uiomove(data + start_offset, data_len, uio);
    } else if ((data_len >= 0) && (data == NULL)) {
        error = EFAULT;
    } else if ((data_len <= 0) && (data != NULL)) {
        error = EIO;
    } else {
        error = ENOTSUP;
    }

    return error;
}
