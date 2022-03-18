//
//  procfs_data.c
//  ProcFS
//
//  Created by Kim Topley on 1/10/16.
//
//
// The functions in this file provide the data for files in the procfs
// file system and the size values for all procfs nodes, as seen in the
// st_size field of the stat structure. Both the data and the size depend
// on the node type. The correct function for each node is specified in
// its procfs_structure_node_t.
//

#include <libkern/libkern.h>
#include <libkext/libkext.h>
#include <sys/bsdtask_info.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <miscfs/procfs/procfs.h>

#include "symdecls.h"

#pragma mark -
#pragma mark Process and Thread Node Data

/*
 * Reads the data for the "pid" node. The data is the
 * owning process's pid.
 */
int
procfs_read_pid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int pid = pnp->node_id.nodeid_pid;
    int error = procfs_copy_data((char *)&pid, sizeof(pid), uio);
    return error;
}

/*
 * Reads the data for the "ppid" node. The data is the
 * owning process's parent's pid.
 */
int
procfs_read_ppid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        int ppid = proc_ppid(p);
        error = procfs_copy_data((char *)&ppid, sizeof(ppid), uio);
    } else {
        error = ESRCH;
    }
    proc_rele(p);

    return error;
}

/*
 * Reads the data for the "pgid" node. The data is the
 * owning process's process group id.
 */
int
procfs_read_pgid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        pid_t pgrpid = proc_pgrpid(p);
        error = procfs_copy_data((char *)&pgrpid, sizeof(pgrpid), uio);
    } else {
        error = ESRCH;
    }
    proc_rele(p);

    return error;
}

/*
 * Reads the data for the "sid" node. The data is the
 * owning process's session id.
 */
int
procfs_read_sid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    int error = 0;
    pid_t session_id = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        _proc_list_lock();
        struct pgrp *pgrp = p->p_pgrp;
        if (pgrp != NULL) {
            struct session *sp = pgrp->pg_session;
            if (sp != NULL) {
                session_id = sp->s_sid;
            }
        }
        _proc_list_unlock();

        error = procfs_copy_data((char *)&session_id, sizeof(session_id), uio);
    } else {
        error = ESRCH;
    }
    proc_rele(p);

    return error;
}

/*
 * Reads the data for the "tty" node. The data is the
 * name of the owning process's controlling terminal.
 */
int
procfs_read_tty_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        _proc_list_lock();
        struct pgrp * pgrp = p->p_pgrp;
        if (pgrp != NULL) {
            // Get the controlling terminal vnode from the process session,
            // if it has one.
            struct session *sp = pgrp->pg_session;
            if (sp != NULL) {
                vnode_t cttyvp;
                cttyvp = sp->s_ttyvp;
                if (cttyvp != NULL) {
                    // Convert the vnode to a full path.
                    int name_len = MAXPATHLEN;
                    char name_buf[MAXPATHLEN + 1];
                    
                    vn_getpath(cttyvp, name_buf, &name_len);
                    error = procfs_copy_data(name_buf, name_len, uio);
                }
            }
        }
        _proc_list_unlock();
    } else {
        error = ESRCH;
    }
    proc_rele(p);

    return error;
}

/*
 * Recreation of proc_pidshortbsdinfo from bsd/kern/proc_info.c
 */
STATIC int
procfs_pidshortbsdinfo(proc_t p, struct proc_bsdshortinfo * pbsd_shortp, int zombie)
{
    unsigned int proc_flag = p->p_flag;
    uint32_t proc_status = (uint32_t)p->p_stat;

    bzero(pbsd_shortp, sizeof(struct proc_bsdshortinfo));
    pbsd_shortp->pbsi_pid = (uint32_t)proc_pid(p);
    pbsd_shortp->pbsi_ppid = (uint32_t)proc_ppid(p);
    pbsd_shortp->pbsi_pgid = (uint32_t)proc_pgrpid(p);
    pbsd_shortp->pbsi_status = proc_status;

    bcopy(&p->p_comm, &pbsd_shortp->pbsi_comm[0], MAXCOMLEN);
    pbsd_shortp->pbsi_comm[MAXCOMLEN - 1] = '\0';

    kauth_cred_t my_cred = kauth_cred_proc_ref(p);
    pbsd_shortp->pbsi_uid = kauth_cred_getuid(my_cred);
    pbsd_shortp->pbsi_gid = kauth_cred_getgid(my_cred);
    pbsd_shortp->pbsi_ruid = kauth_cred_getruid(my_cred);
    pbsd_shortp->pbsi_rgid = kauth_cred_getrgid(my_cred);
    pbsd_shortp->pbsi_svuid = kauth_cred_getsvuid(my_cred);
    pbsd_shortp->pbsi_svgid = kauth_cred_getsvgid(my_cred);
    kauth_cred_unref(&my_cred);

    pbsd_shortp->pbsi_flags = 0;

    if ((proc_flag & P_SYSTEM) == P_SYSTEM) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_SYSTEM;
    }

    if ((proc_flag & P_LTRACED) == P_LTRACED) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_TRACED;
    }

    if (proc_exiting(p) != 0) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_INEXIT;
    }

    if ((proc_flag & P_LPPWAIT) == P_LPPWAIT) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PPWAIT;
    }

    if (proc_is64bit(p) != 0) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_LP64;
    }

    if ((proc_flag & P_CONTROLT) == P_CONTROLT) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_CONTROLT;
    }

    if ((proc_flag & P_THCWD) == P_THCWD) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_THCWD;
    }

    if (_proc_issetugid(p) != 0)  {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PSUGID;
    }

    if ((proc_flag & P_EXEC) == P_EXEC) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_EXEC;
    }

    if ((proc_flag & P_DELAYIDLESLEEP) == P_DELAYIDLESLEEP) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_DELAYIDLESLEEP;
    }

    switch (PROC_CONTROL_STATE(p)) {
    case P_PCTHROTTLE:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PC_THROTTLE;
        break;
    case P_PCSUSP:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PC_SUSP;
        break;
    case P_PCKILL:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PC_KILL;
        break;
    };

    switch (PROC_ACTION_STATE(p)) {
    case P_PCTHROTTLE:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PA_THROTTLE;
        break;
    case P_PCSUSP:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PA_SUSP;
        break;
    };

    /* if process is a zombie skip bg state */
    // FIXME:
    if ((zombie == 0) && (proc_status != SZOMB) && (_proc_task(p) != TASK_NULL)) {
        _proc_get_darwinbgstate(_proc_task(p), &pbsd_shortp->pbsi_flags);
    }

    return 0;
}

/*
 * Reads basic info for a process. Populates an instance of the proc_bsdinfo
 * structure and copies it to the area described by a uio structure.
 */
int
procfs_read_proc_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    // Get the process id from the node id in the procfsnode and locate
    // the process.
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        struct proc_bsdshortinfo info; // Mac OS X >= 10.7 has proc_bsdshortinfo

        // Get the BSD-centric process info and copy it out.
        error = procfs_pidshortbsdinfo(p, &info, 0);
        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
        } else {
            error = ESRCH;
        } 
    }
    proc_rele(p);

    return error;
}

/*
 * Recreation of proc_pidtaskinfo from bsd/kern/proc_info.c
 */
STATIC int
procfs_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo)
{
    bzero(ptinfo, sizeof(struct proc_taskinfo));
    _fill_taskprocinfo(_proc_task(p), (struct proc_taskinfo_internal *)ptinfo);

    return 0;
}

/*
 * Reads basic info for the Mach task associated with a process. Populates 
 * an instance of a proc_taskinfo structure and copies it to tthe area described
 * by a uio structure.
 */
int
procfs_read_task_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    // Get the process id from the node id in the procfsnode and locate
    // the process.
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        struct proc_taskinfo info;

        // Get the task info and copy it out.
        error = procfs_pidtaskinfo(p, &info);
        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
        } else {
            error = ESRCH;
        }
    }
    proc_rele(p);

    return error;
}

/*
 * Recreation of proc_pidthreadinfo from bsd/kern/proc_info.c
 */
STATIC int
procfs_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo)
{
    bzero(pthinfo, sizeof(struct proc_threadinfo));
    _fill_taskthreadinfo(_proc_task(p), (uint64_t)arg, thuniqueid, (struct proc_threadinfo_internal *)pthinfo, NULL, NULL);

    return 0;
}

/*
 * Reads basic info for a thread. Populates an instance of a proc_threadinfo 
 * structure and copies it to tthe area described by a uio structure.
 */
int
procfs_read_thread_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    // Get the process id and thread from the node id in the procfsnode and locate
    // the process.
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        struct proc_threadinfo info;
        uint64_t threadid = pnp->node_id.nodeid_objectid;
        
        // Get the task info and copy it out.
        error = procfs_pidthreadinfo(p, threadid, TRUE, &info);
        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
        } else {
            error = ESRCH;
        } 
    }
    proc_rele(p);

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
procfs_read_fd_data(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx)
{
    // We need the file descriptor and the process id. We get
    // both of them from the node id.
    int pid = pnp->node_id.nodeid_pid;
    //uint64_t fd = (int)pnp->node_id.nodeid_objectid;
    
    int error = 0;
    proc_t p = proc_find(pid);
    if (p != NULL) {
#if 0
        struct fileproc *fp;
        vnode_t vp;
        uint32_t vid;
        
        // Get the vnode, vnode id and fileproc structure for the file.
        // The fileproc has an additional iocount, which we must remember
        // to release.
        if ((error = file_vnode_withvid(fd, &vp, &vid)) == 0) {
            // Get a hold on the vnode and check that it did not
            // change id.
            if ((error = vnode_getwithvid(vp, vid)) == 0) {
                // Got the vnode. Pack vnode and file info into
                // a vnode_fdinfowithpath structure.
                struct vnode_fdinfowithpath info;
                bzero(&info, sizeof(info));
                fill_fileinfo(fp, p, fd, &info.pfi);
                error = procfs_fill_vnodeinfo(vp, &info.pvip.vip_vi, ctx);
                // If all is well, add in the file path and copy the data
                // out to user space.
                if (error == 0) {
                    int count = MAXPATHLEN;
                    vn_getpath(vp, info.pvip.vip_path, &count);
                    info.pvip.vip_path[MAXPATHLEN-1] = 0;
                    error = procfs_copy_data((char *)&info, sizeof(info), uio);
                }
                
                // Release the vnode hold.
                vnode_put(vp);
            }
            
            // Release the hold on the fileproc structure
            file_drop(fd);
        }
#endif
        error = ENOTSUP;
    } else {
        error = ESRCH;
    }
    proc_rele(p);
    
    return error;
}

/*
 * Reads the data associated with a file descriptor that refers to a socket.
 */
int
procfs_read_socket_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    // We need the file descriptor and the process id. We get
    // both of them from the node id.
    int pid = pnp->node_id.nodeid_pid;
    //uint64_t fd = (int)pnp->node_id.nodeid_objectid;

    int error = 0;

    proc_t p = proc_find(pid);
    if (p != NULL) {
#if 0
        struct fileproc *fp;
        socket_t so;

        // Get the socket and fileproc structures for the file. If the
        // file is not a socket, this fails and we will return an error.
        // Otherwise, the fileproc has an additional iocount, which we
        // must remember to release.
        if ((error = file_socket(fd, &so)) == 0) {
            struct socket_fdinfo info;

            bzero(&info, sizeof(info));
            fill_fileinfo(fp, p, fd, &info.pfi);
            if ((error = _fill_socketinfo(so, &info.psi)) == 0) {
                error = procfs_copy_data((char *)&info, sizeof(info), uio);
            }

            // Release the hold on the fileproc structure
            file_drop(fd);
        }
#endif
        error = ENOTSUP;
    } else {
        error = ESRCH;
    }
    proc_rele(p);

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
procfs_get_node_size_attr(procfsnode_t *pnp, kauth_cred_t creds)
{
    procfs_structure_node_t *snode = pnp->node_structure_node;
    procfs_structure_node_type_t node_type = snode->psn_node_type;

    // In the special cases of "." and "..", we need to first move up
    // to the parent and grandparent structure node to get the correct result.
    if (node_type == PROCFS_DIR_THIS) {
        snode = snode->psn_parent;
    } else if (node_type == PROCFS_DIR_PARENT) {
        snode = snode->psn_parent;
        if (snode != NULL && snode->psn_node_type != PROCFS_ROOT) {
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
        procfs_structure_node_t *next_snode;
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
procfs_process_node_size(procfsnode_t *pnp, kauth_cred_t creds)
{
    // Nodes of this type contribute a size of 1 for each visible process.
    size_t size = 0;
    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
    if (p != NULL) {
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
procfs_thread_node_size(procfsnode_t *pnp, __unused kauth_cred_t creds)
{
    // Nodes of this type contribute a size of 1 for each thread
    // in the owning process. Because of the way the file system is
    // structured, the pid of the owning process is available in the
    // node_id of the procfs node.
    size_t size = 0;
    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        task_t task = _proc_task(p);
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
procfs_fd_node_size(procfsnode_t *pnp, __unused kauth_cred_t creds)
{
    size_t count = 0;
    struct fileproc *fp;

    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);

    if (p == NULL) {
        count = 0;
    } else {
        _proc_fdlock_spin(p);

        _fdt_foreach(fp, p) {
            count++;
        }
        _proc_fdunlock(p);
    }
    proc_rele(p);

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
    if (data_len >= 0) {
        error = uiomove(data + start_offset, (int)data_len, uio);
    }
    return error;
}
