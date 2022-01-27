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
#include <sys/file_internal.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/uio_internal.h>
#include <sys/vnode_internal.h>
#include "procfsnode.h"
#include "procfsstructure.h"
#include "procfs_data.h"
#include "procfs_subr.h"

#pragma mark -
#pragma mark Local Function Prototypes

STATIC int procfs_copy_data(char *data, int data_len, uio_t uio);

#pragma mark -
#pragma mark External References

extern int proc_pidbsdinfo(proc_t p, struct proc_bsdinfo *pinfo, int zombie);
extern int proc_pidtaskinfo(proc_t p, struct proc_taskinfo *tinfo);
extern int proc_pidthreadinfo(proc_t p, uint64_t threadid,  int thuniqueid, struct proc_threadinfo *info);
extern int fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo);
extern void  fill_fileinfo(struct fileproc * fp, proc_t proc, int fd, struct proc_fileinfo * finfo);

#pragma mark -
#pragma mark Process and Thread Node Data

/*
 * Reads the data for the "pid" node. The data is the
 * owning process's pid.
 */
int
procfs_read_pid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    pid_t pid = pnp->node_id.nodeid_pid;
    int error = procfs_copy_data((char *)&pid, sizeof(pid), uio);
    return error;
}

/*
 * Reads the data for the "ppid" node. The data is the
 * owning process's parent's pid.
 */
int
procfs_read_ppid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    int error;
    
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        pid_t ppid = p->p_ppid;
        error = procfs_copy_data((char *)&ppid, sizeof(ppid), uio);
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
procfs_read_pgid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    int error;
    
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        pid_t pgrpid = p->p_pgrpid;
        error = procfs_copy_data((char *)&pgrpid, sizeof(pgrpid), uio);
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
procfs_read_sid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    int error;
    
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        pid_t session_id = (pid_t)0;
        proc_list_lock();
        struct pgrp *pgrp = p->p_pgrp;
        if (pgrp != NULL) {
            struct session *sp = pgrp->pg_session;
            if (sp != NULL) {
                session_id = sp->s_sid;
            }
        }
        proc_list_unlock();
        
        error = procfs_copy_data((char *)&session_id, sizeof(session_id), uio);
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
procfs_read_tty_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    int error = 0;
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        proc_list_lock();
        struct pgrp *pgrp = p->p_pgrp;
        if (pgrp != NULL) {
            // Get the controlling terminal vnode from the process session,
            // if it has one.
            struct session *sp = pgrp->pg_session;
            if (sp != NULL) {
                vnode_t cttyvp;
                session_lock(sp);
                cttyvp = sp->s_ttyvp;
                session_unlock(sp);
                if (cttyvp != NULL) {
                    // Convert the vnode to a full path.
                    int name_len = MAXPATHLEN;
                    char name_buf[MAXPATHLEN + 1];
                    
                    vn_getpath(cttyvp, name_buf, &name_len);
                    error = procfs_copy_data(name_buf, name_len, uio);
                }
            }
        }
        proc_list_unlock();
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
procfs_read_proc_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    // Get the process id from the node id in the procfsnode and locate
    // the process.
    int error = 0;
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        struct proc_bsdinfo info;
        
        // Get the BSD-centric process info and copy it out.
        error = proc_pidbsdinfo(p, &info, FALSE);
        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
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
procfs_read_task_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    // Get the process id from the node id in the procfsnode and locate
    // the process.
    int error = 0;
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        struct proc_taskinfo info;
        
        // Get the task info and copy it out.
        error = proc_pidtaskinfo(p, &info);
        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
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
procfs_read_thread_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    // Get the process id and thread from the node id in the procfsnode and locate
    // the process.
    int error = 0;
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        struct proc_threadinfo info;
        uint64_t threadid = pnp->node_id.nodeid_objectid;
        
        // Get the task info and copy it out.
        error  = proc_pidthreadinfo(p, threadid, TRUE, &info);
        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
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
procfs_read_fd_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    // We need the file descriptor and the process id. We get
    // both of them from the node id.
    pid_t pid = pnp->node_id.nodeid_pid;
    int fd = (int)pnp->node_id.nodeid_objectid;
    
    int error = 0;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        struct fileproc *fp;
        vnode_t vp;
        uint32_t vid;
        
        // Get the vnode, vnode id and fileproc structure for the file.
        // The fileproc has an additional iocount, which we must remember
        // to release.
        if ((error = fp_getfvpandvid(p, fd, &fp, &vp, &vid)) == 0) {
            // Get a hold on the vnode and check that it did not
            // change id.
            if ((error = vnode_getwithvid(vp, vid)) == 0) {
                // Got the vnode. Pack vnode and file info into
                // a vnode_fdinfowithpath structure.
                struct vnode_fdinfowithpath info;
                bzero(&info, sizeof(info));
                
                fill_fileinfo(fp, p, fd, &info.pfi);
                error = fill_vnodeinfo(vp, &info.pvip.vip_vi);
                
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
            fp_drop(p, fd, fp, FALSE);
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
procfs_read_socket_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    // We need the file descriptor and the process id. We get
    // both of them from the node id.
    pid_t pid = pnp->node_id.nodeid_pid;
    int fd = (int)pnp->node_id.nodeid_objectid;
    
    int error = 0;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        struct fileproc *fp;
        socket_t so;
        
        // Get the socket and fileproc structures for the file. If the
        // file is not a socket, this fails and we will return an error.
        // Otherwise, the fileproc has an additional iocount, which we
        // must remember to release.
        if ((error = fp_getfsock(p, fd, &fp, &so)) == 0) {
            struct socket_fdinfo info;
            
            bzero(&info, sizeof(info));
            fill_fileinfo(fp, p, fd, &info.pfi);
            if ((error = fill_socketinfo(so, &info.psi)) == 0) {
                error = procfs_copy_data((char *)&info, sizeof(info), uio);
            }

            // Release the hold on the fileproc structure
            fp_drop(p, fd, fp, FALSE);
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
procfs_get_node_size_attr(procfsnode_t *pnp, kauth_cred_t creds) {
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
procfs_process_node_size(__unused procfsnode_t *pnp, kauth_cred_t creds) {
    // Nodes of this type contribute a size of 1 for each visible process.
    return procfs_get_process_count(creds);
}

/*
 * Gets the size for a node that represents a thread.
 */
size_t
procfs_thread_node_size(procfsnode_t *pnp, __unused kauth_cred_t creds) {
    // Nodes of this type contribute a size of 1 for each thread
    // in the owning process. Because of the way the file system is
    // structured, the pid of the owning process is available in the
    // node_id of the procfs node.
    int size = 0;
    pid_t pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        task_t task = proc_task(p);
        if (task != NULL) {
            size += procfs_get_task_thread_count(task);
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
procfs_fd_node_size(procfsnode_t *pnp, __unused kauth_cred_t creds) {
    int size = 0;
    pid_t pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        // Count the open files in this process.
        struct filedesc *fdp = p->p_fd;
        proc_fdlock_spin(p);
        for (int i = 0; i < fdp->fd_nfiles; i++) {
            struct fileproc *fp = fdp->fd_ofiles[i];
            if (fp != NULL && !(fdp->fd_ofileflags[i] & UF_RESERVED)) {
                size++;
            }
        }
        proc_fdunlock(p);
        proc_rele(p);
    }
    return size;
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
STATIC int
procfs_copy_data(char *data, int data_len, uio_t uio) {
    int error = 0;
    off_t start_offset = uio->uio_offset;
    data_len -= start_offset;
    if (data_len >= 0) {
        error = uiomove(data + start_offset, (int)data_len, uio);
    }
    return error;
}


