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

#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <miscfs/procfs/procfs.h>
#include <miscfs/procfs/procfs_data.h>
#include <miscfs/procfs/procfs_node.h>
#include <miscfs/procfs/procfs_structure.h>
#include <miscfs/procfs/procfs_subr.h>

#include "procfs_info.h"
#include "procfs_internal.h"

#pragma mark -
#pragma mark Local Function Prototypes

STATIC int procfs_copy_data(char *data, int data_len, uio_t uio);

#pragma mark -
#pragma mark Process and Thread Node Data

/*
 * Reads the data for the "pid" node. The data is the
 * owning process's pid.
 */
int
procfs_read_pid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    pid_t pid = pnp->node_id.nodeid_pid;
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
    int error;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        int ppid = proc_ppid(p);
        error = procfs_copy_data((char *)&ppid, sizeof(ppid), uio);
        proc_rele(p);
        if (p != NULL) {
            p = NULL;
        }
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
procfs_read_pgid_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        pid_t pgrpid = proc_pgrpid(p);
        error = procfs_copy_data((char *)&pgrpid, sizeof(pgrpid), uio);
        proc_rele(p);
        if (p != NULL) {
            p = NULL;
        }
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
        proc_rele(p);
        if (p != NULL) {
            p = NULL;
        }
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
procfs_read_proc_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    // Get the process id from the node id in the procfsnode and locate
    // the process.
    int error = 0;
    char smallname[128];
    char *filename = NULL;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        struct proc_bsdshortinfo info; // Mac OS X >= 10.7 has proc_bsdshortinfo

        // Get the BSD-centric process info and copy it out.
        error = procfs_proc_pidshortbsdinfo(p, &info, 0);
        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
        }
        proc_rele(p);
        if (p != NULL) {
            p = NULL;
        }

        uint32_t pid = info.pbsi_pid;
        uint32_t ppid = info.pbsi_ppid;
        uint32_t pgid = info.pbsi_pgid;
        uint32_t status = info.pbsi_status;
        char comm = info.pbsi_comm[0];
        uint32_t flags = info.pbsi_flags;
        uid_t uid = info.pbsi_uid;
        gid_t gid = info.pbsi_gid;
        uid_t ruid = info.pbsi_ruid;
        gid_t rgid = info.pbsi_rgid;
        uid_t svuid = info.pbsi_svuid;
        gid_t svgid = info.pbsi_svgid;

        filename = &smallname[0];
        snprintf(filename, sizeof(smallname),
                "pid\t\t: %u\n", pid,
                "ppid\t\t: %u\n", ppid,
                "pgid\t\t: %u\n", pgid,
                "status\t\t: %u\n", status,
                "comm\t\t: %s\n", comm,
                "flags\t\t: %u\n", flags,
                "uid\t\t: %u\n", uid,
                "gid\t\t: %u\n", gid,
                "ruid\t\t: %u\n", ruid,
                "rgid\t\t: %u\n", rgid,
                "svuid\t\t: %u\n", svuid,
                "svgid\t\t: %u\n", svgid);
    }
    
    return error;
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
    char smallname[128];
    char *filename = NULL;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        struct proc_taskinfo info;

        // Get the task info and copy it out.
        error = procfs_proc_pidtaskinfo(p, &info);
        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
        }
        proc_rele(p);
        if (p != NULL) {
            p = NULL;
        }

        uint64_t virtual_size = info.pti_virtual_size;
        uint64_t resident_size = info.pti_resident_size;
        uint64_t total_user = info.pti_total_user;
        uint64_t total_system = info.pti_total_system;
        uint64_t threads_user = info.pti_threads_user;
        uint64_t threads_system = info.pti_threads_system;
        int32_t policy = info.pti_policy;
        int32_t faults = info.pti_faults;
        int32_t pageins = info.pti_pageins;
        int32_t cow_faults = info.pti_cow_faults;
        int32_t messages_sent = info.pti_messages_sent;
        int32_t messages_received = info.pti_messages_received;
        int32_t syscalls_mach = info.pti_syscalls_mach;
        int32_t syscalls_unix = info.pti_syscalls_unix;
        int32_t csw = info.pti_csw;
        int32_t threadnum = info.pti_threadnum;
        int32_t numrunning = info.pti_numrunning;
        int32_t priority = info.pti_priority;

        filename = &smallname[0];
        snprintf(filename, sizeof(smallname),
                "virtual size\t\t: %llu\n", virtual_size,
                "resident size\t\t: %llu\n", resident_size,
                "total user\t\t: %llu\n", total_user,
                "total system\t\t: %llu\n", total_system,
                "threads user\t\t: %llu\n", threads_user,
                "threads system\t\t: %llu\n", threads_system,
                "policy\t\t: %d\n", policy,
                "faults\t\t: %d\n", faults,
                "pageins\t\t: %d\n", pageins,
                "cow faults\t\t: %d\n", cow_faults,
                "messages sent\t\t: %d\n", messages_sent,
                "messages received\t\t: %d\n", messages_received,
                "syscalls mach\t\t: %d\n", syscalls_mach,
                "syscalls unix\t\t: %d\n", syscalls_unix,
                "csw\t\t: %d\n", csw,
                "threadnum\t\t: %d\n", threadnum,
                "numrunning\t\t: %d\n", numrunning,
                "priority\t\t: %d\n", priority);
    }

    return error;
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
    char smallname[128];
    char *filename = NULL;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p != NULL) {
        struct proc_threadinfo info;
        uint64_t threadid = pnp->node_id.nodeid_objectid;
        
        // Get the task info and copy it out.
        error  = procfs_proc_pidthreadinfo(p, threadid, TRUE, &info);
        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
        }
        proc_rele(p);
        if (p != NULL) {
            p = NULL;
        }

        uint64_t user_time = info.pth_user_time;
        uint64_t system_time = info.pth_system_time;
        int32_t cpu_usage = info.pth_cpu_usage;
        int32_t policy = info.pth_policy;
        int32_t run_state = info.pth_run_state;
        int32_t flags = info.pth_flags;
        int32_t sleep_time = info.pth_sleep_time;
        int32_t curpri = info.pth_curpri;
        int32_t priority = info.pth_priority;
        int32_t maxpriority = info.pth_maxpriority;
        char name = info.pth_name[0];

        filename = &smallname[0];
        snprintf(filename, sizeof(smallname),
                "user time\t\t: %llu\n", user_time,
                "system time\t\t: %llu\n", system_time,
                "cpu usage\t\t: %d\n", cpu_usage,
                "policy\t\t: %d\n", policy,
                "run state\t\t: %d\n", run_state,
                "flags\t\t: %d\n", flags,
                "sleep time\t\t: %d\n", sleep_time,
                "curpri\t\t: %d\n", curpri,
                "priority\t\t: %d\n", priority,
                "maxpriority\t\t: %d\n", maxpriority,
                "name\t\t: %s\n", name);
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
procfs_read_fd_data(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx) {
    // We need the file descriptor and the process id. We get
    // both of them from the node id.
    pid_t pid = pnp->node_id.nodeid_pid;
    int fd = (int)pnp->node_id.nodeid_objectid;
    
    int error = 0;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        struct fileproc *fp = NULL;
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
                //fill_fileinfo(fp, p, fd, &info.pfi);
                procfs_fill_fileinfo(fp, p, fd, &info.pfi);
                //error = fill_vnodeinfo(vp, &info.pvip.vip_vi);
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
            //fp_drop(p, fd, fp, FALSE); // can't be resolved
            file_drop(fd);
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
procfs_read_socket_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    // We need the file descriptor and the process id. We get
    // both of them from the node id.
    pid_t pid = pnp->node_id.nodeid_pid;
    int fd = (int)pnp->node_id.nodeid_objectid;

    int error = 0;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        struct fileproc *fp = NULL;
        socket_t so;

        // Get the socket and fileproc structures for the file. If the
        // file is not a socket, this fails and we will return an error.
        // Otherwise, the fileproc has an additional iocount, which we
        // must remember to release.
        if ((error = file_socket(fd, &so)) == 0) {
            struct socket_fdinfo info;

            bzero(&info, sizeof(info));
            procfs_fill_fileinfo(fp, p, fd, &info.pfi);
            if ((error = _fill_socketinfo(so, &info.psi)) == 0) {
                error = procfs_copy_data((char *)&info, sizeof(info), uio);
            }

            // Release the hold on the fileproc structure
            //fp_drop(p, fd, fp, FALSE); // can't be resolved
            file_drop(fd);
        }
        proc_rele(p);
        if (p != NULL) {
            p = NULL;
        }
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
    pid_t pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        if (size == 0) {
            size += sizeof(procfs_get_process_count(creds));
        }
        proc_rele(p);
        if (p != NULL) {
            p = NULL;
        }
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
    pid_t pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        task_t task = _proc_task(p);
        if (task != NULL) {
            size += sizeof(procfs_get_task_thread_count(task));
        }
        proc_rele(p);
        if (p != NULL) {
            p = NULL;
        }
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
    pid_t pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);

    if (p == NULL) {
        return 0;
    }

    size_t count = 0;

    _proc_fdlock_spin(p);

    struct fileproc *iter;
    _fdt_foreach(iter, p) {
        count++;
    }

    _proc_fdunlock(p);
    proc_rele(p);
    if (p != NULL) {
        p = NULL;
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
STATIC int
procfs_copy_data(char *data, int data_len, uio_t uio)
{
    int error = 0;
    off_t start_offset = uio_offset(uio);
    data_len -= start_offset;
    if (data_len >= 0) {
        error = uiomove(data + start_offset, (int)data_len, uio);
    }
    return error;
}


