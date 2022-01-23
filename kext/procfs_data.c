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
// its struct procfs_structure_node.
//

#include <kern/locks.h>

#include <libkern/libkern.h>

#include <sys/file.h>
#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/sysent.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include "procfs_data.h"
#include "procfs_fillinfo.h"
#include "procfs_proc.h"
#include "procfs_locks.h"
#include "procfs_node.h"
#include "procfs_structure.h"
#include "procfs_subr.h"


#pragma mark -
#pragma mark External References

extern task_t proc_task(proc_t);
extern proc_t proc_find(int pid);


#pragma mark -
#pragma mark Local Function Prototypes

static int procfs_copy_data(char *data, int data_len, uio_t uio);


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
        int ppid = proc_ppid(p);
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
        pid_t pgrpid = proc_pgrpid(p);
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

        procfs_list_lock();
        proc_t pgrp = proc_find(proc_pgrpid(p));
        if (pgrp != NULL) {
            session_id = proc_sessionid(pgrp);
            if (session_id < 0) {
                session_id = 0; 
            }
        }
        procfs_list_unlock();
        
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
        procfs_list_lock();
        pid_t pgrpid = proc_pgrpid(p);
        proc_t pgrp = proc_find(pgrpid);
        if (pgrp != NULL) {
            // Get the controlling terminal vnode from the process session,
            // if it has one.
            pid_t sessionid = proc_sessionid(pgrp);
            proc_t session = proc_find(sessionid);
            if (session != NULL) {
                vnode_t cttyvp;
                int err = proc_gettty(session, &cttyvp);
                if (err == 0) {
                    // Convert the vnode to a full path.
                    int name_len = MAXPATHLEN;
                    char name_buf[MAXPATHLEN + 1];
                    
                    vn_getpath(cttyvp, name_buf, &name_len);
                    error = procfs_copy_data(name_buf, name_len, uio);
                }
            }
        }
        procfs_list_unlock();
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
procfs_read_proc_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;
    int gotref = 0;

    struct session *sessionp;
    struct pgrp *pg;
    struct tty *tp;
    struct proc_bsdinfo *pbsd;

    // Get the process id from the node id in the procfsnode and locate
    // the process.
    proc_t p = proc_find(pnp->node_id.nodeid_pid);

    if (p != NULL) {
        // Get the BSD-centric process info and copy it out.
        kauth_cred_t cred;
        cred = kauth_cred_proc_ref(p);
        bzero(pbsd, sizeof(struct proc_bsdinfo));
        uint32_t status = p->p_stat; // FIXME: We want to avoid calling the proc structure as much as we can
        uint32_t xstatus = p->p_xstat; // FIXME: We want to avoid calling the proc structure as much as we can
        uint32_t pid = proc_pid(p);
        uint32_t ppid = proc_ppid(p);
        uid_t uid = kauth_cred_getuid(cred);
        gid_t gid = kauth_cred_getgid(cred);
        uid_t ruid = kauth_cred_getruid(cred);
        gid_t rgid = kauth_cred_getrgid(cred);
        uid_t svuid = kauth_cred_getsvuid(cred);
        gid_t svgid = kauth_cred_getsvgid(cred);
        kauth_cred_unref(&cred);

        bcopy(&p->p_comm, &pbsd->pbi_comm[0], MAXCOMLEN); // FIXME: We want to avoid calling the proc structure as much as we can
        char comm = pbsd->pbi_comm[MAXCOMLEN - 1] = '\0';
        bcopy(&p->p_name, &pbsd->pbi_name[0], 2 * MAXCOMLEN); // FIXME: We want to avoid calling the proc structure as much as we can
        char name = pbsd->pbi_name[(2 * MAXCOMLEN) - 1] = '\0';

        uint32_t nfiles = 0;

        int zombie = 0;
        int findzomb = 0;

        if ((p = proc_find(pid)) == PROC_NULL) {
            if (findzomb) {
                p = procfs_proc_find_zombref(pid);
            }
            if (p == PROC_NULL) {
                error = ESRCH;
            }
            zombie = 1;
        }

        if (zombie == 0) {
            nfiles = p->p_fd->fd_nfiles;
        }
#if 0
        /* if process is a zombie skip bg state */
        if ((zombie == 0) && (p->p_stat != SZOMB) && (p->task != TASK_NULL)) {
            proc_get_darwinbgstate(p->task, &pbsd->pbi_flags);
        }
#endif
        uint32_t pgid = pbsd->pbi_pgid;
        uint32_t pjobc = pbsd->pbi_pjobc;

        uint32_t tdev = NODEV;
        uint32_t tpgid = 0;
        if (pg != PGRP_NULL) {
            pgid = p->p_pgrpid; // FIXME: We want to avoid calling the proc structure as much as we can
            pjobc = pg->pg_jobc; // FIXME: We want to avoid calling the pgrp structure as much as we can
            if ((p->p_flag & P_CONTROLT) && (sessionp != SESSION_NULL) && (tp = SESSION_TP(sessionp))) {
                tdev = proc_gettty_dev(p, &tdev);
                tpgid = sessionp->s_ttypgrpid;
            }
        }

        int32_t nice = p->p_nice; // FIXME: We want to avoid calling the proc structure as much as we can
        uint64_t start_tvsec = p->p_start.tv_sec; // FIXME: We want to avoid calling the proc structure as much as we can
        uint64_t start_tvusec = p->p_start.tv_usec; // FIXME: We want to avoid calling the proc structure as much as we can

        uint32_t flags = 0;
        int issystem = 0; // FIXME
        if (issystem == P_SYSTEM) {
            flags |= PROC_FLAG_SYSTEM;
        }
        int istraced = 0; // FIXME
        if (istraced == P_LTRACED) {
            flags |= PROC_FLAG_TRACED;
        }
        int isexit = 0; // FIXME
        if (isexit == P_LEXIT) {
            flags |= PROC_FLAG_INEXIT;
        }
        int isppwait = 0; // FIXME
        if (isppwait == P_LPPWAIT) {
            flags |= PROC_FLAG_PPWAIT;
        }
        int is64bit = proc_is64bit(p);
        if (is64bit == P_LP64) {
            flags |= PROC_FLAG_LP64;
        }
        int iscontrolt = 0; // FIXME
        if (iscontrolt == P_CONTROLT) {
            flags |= PROC_FLAG_CONTROLT;
        }
        int isthcwd = 0; // FIXME
        if (isthcwd == P_THCWD) {
            flags |= PROC_FLAG_THCWD;
        }
        int issugid = 0; // FIXME
        if (issugid == P_SUGID) {
            flags |= PROC_FLAG_PSUGID;
        }
        int isexec = 0; // FIXME
        if (isexec == P_EXEC) {
            flags |= PROC_FLAG_EXEC;
        }
        int isctty = proc_gettty(p, &tp);
        if (sessionp != SESSION_NULL) {
            if (SESS_LEADER(p, sessionp)) {
                flags |= PROC_FLAG_SLEADER;
            }
            if (isctty != 0) {
                flags |= PROC_FLAG_CTTY;
            }
        }

        switch (PROC_CONTROL_STATE(p)) {
        case P_PCTHROTTLE:
            flags |= PROC_FLAG_PC_THROTTLE;
            break;
        case P_PCSUSP:
            flags |= PROC_FLAG_PC_SUSP;
            break;
        case P_PCKILL:
            flags |= PROC_FLAG_PC_KILL;
            break;
        };

        switch (PROC_ACTION_STATE(p)) {
            case P_PCTHROTTLE:
            flags |= PROC_FLAG_PA_THROTTLE;
            break;
        case P_PCSUSP:
            flags |= PROC_FLAG_PA_SUSP;
            break;
        };

        struct proc_bsdinfo info = {
            .pbi_flags               = flags,           /* 64bit; emulated etc */
            .pbi_status              = status,          /* status: p_stat value, SZOMB, SRUN, etc */
            .pbi_xstatus             = xstatus,         /* x status */
            .pbi_pid                 = pid,             /* process id */
            .pbi_ppid                = ppid,            /* process parent id */
            .pbi_uid                 = uid,             /* current uid on process */
            .pbi_gid                 = gid,             /* current gid on process */
            .pbi_ruid                = ruid,            /* current ruid on process */
            .pbi_rgid                = rgid,            /* current rgid on process */
            .pbi_svuid               = svuid,           /* current svuid on process */
            .pbi_svgid               = svgid,           /* current svgid on process */
            .pbi_comm                = {comm},           /* upto 16 characters of process name */
            .pbi_name                = {name},           /* empty if no name is registered */
            .pbi_nfiles              = nfiles,          /* number of files */
            .pbi_pgid                = pgid,            /* current parent gid */
            .pbi_pjobc               = pjobc,           /* pjobc */
            .e_tdev                  = tdev,            /* controlling tty dev */
            .e_tpgid                 = tpgid,           /* tty process group id */
            .pbi_nice                = nice,            /* nice */
            .pbi_start_tvsec         = start_tvsec ,    /* start tv sec */
            .pbi_start_tvusec        = start_tvusec,    /* Start tv micro sec */
        };

        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
        }
        proc_rele(p);
    }
    if (sessionp != SESSION_NULL) {
        procfs_session_rele(sessionp);
    }
#if 0
    if (pg != PGRP_NULL) {
        pg_rele(pg);
    }
#endif
    return error;
}

/*
 * Reads basic info for the Mach task associated with a process. Populates 
 * an instance of a proc_taskinfo structure and copies it to tthe area described
 * by a uio structure.
 */
int
procfs_read_task_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    int error = 0;

    // Get the process id from the node id in the procfsnode and locate
    // the process.
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    uint64_t threadid = pnp->node_id.nodeid_objectid;

    uint64_t virtual_size = 0;
    uint64_t resident_size = 0;
    uint64_t total_user = 0;
    uint64_t total_system = 0;
    uint64_t threads_user = 0;
    uint64_t threads_system = 0;
    int32_t policy = 0;
    int32_t faults = 0;
    int32_t pageins = 0;
    int32_t cow_faults = 0;
    int32_t messages_sent = 0;
    int32_t messages_received = 0;
    int32_t syscalls_mach = 0;
    int32_t syscalls_unix = 0;
    int32_t csw = 0;
    int32_t threadnum = 0;
    int32_t numrunning = 0;
    int32_t priority = 0;

    if (p != NULL) {
        // Get the task info and copy it out.
        struct proc_taskinfo info = {
            .pti_virtual_size       = virtual_size,         /* virtual memory size (bytes) */
            .pti_resident_size      = resident_size,        /* resident memory size (bytes) */
            .pti_total_user         = total_user,           /* total time */
            .pti_total_system       = total_system,
            .pti_threads_user       = threads_user,         /* existing threads only */
            .pti_threads_system     = threads_system,
            .pti_policy             = policy,               /* default policy for new threads */
            .pti_faults             = faults,               /* number of page faults */
            .pti_pageins            = pageins,              /* number of actual pageins */
            .pti_cow_faults         = cow_faults,           /* number of copy-on-write faults */
            .pti_messages_sent      = messages_sent,        /* number of messages sent */
            .pti_messages_received  = messages_received,    /* number of messages received */
            .pti_syscalls_mach      = syscalls_mach,        /* number of mach system calls */
            .pti_syscalls_unix      = syscalls_unix,        /* number of unix system calls */
            .pti_csw                = csw,                  /* number of context switches */
            .pti_threadnum          = threadnum,            /* number of threads in the task */
            .pti_numrunning         = numrunning,           /* number of running threads */
            .pti_priority           = priority,             /* task priority*/
        };

        if (error == 0) {
            error = procfs_copy_data((char *)&info, sizeof(info), uio);
        }
        proc_rele(p);
#if 0
        // FIXME: We don't have session_rele() or pg_rele() (yet?)
        if (sessionp != SESSION_NULL) {
            session_rele(sessionp);
        }
        if (pg != PGRP_NULL) {
            pg_rele(pg);
        }
#endif
    }
    return error;
}

/*
 * Reads basic info for a thread. Populates an instance of a proc_threadinfo 
 * structure and copies it to tthe area described by a uio structure.
 */
int
procfs_read_thread_info(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx) {
    int error = 0;
    int32_t run_state = 0;
    uint32_t flags = 0;

    // Get the process id and thread from the node id in the procfsnode and locate
    // the process.
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    uint64_t threadid = pnp->node_id.nodeid_objectid;

    if (p != NULL) {
        // Get the task info and copy it out.
        //error  = proc_pidthreadinfo(p, threadid, TRUE, &info);

        uint64_t user_time = 0;
        uint64_t system_time = 0;
        int32_t cpu_usage = 0;
        int32_t policy = 0;

        int32_t run_state = 0;
        int isrunning = 0;
        if (isrunning != 0) {
            run_state |= TH_STATE_RUNNING;
        }
        int isstopped = 0;
        if (isstopped != 0) {
            run_state |= TH_STATE_STOPPED;
        }
        int iswaiting = 0;
        if (iswaiting != 0) {
            run_state |= TH_STATE_WAITING;
        }
        int isuninterruptable = 0;
        if (isuninterruptable != 0) {
            run_state |= TH_STATE_UNINTERRUPTIBLE;
        }
        int ishalted = 0;
        if (ishalted != 0) {
            run_state |= TH_STATE_HALTED;
        }

        int32_t flags = 0;
        int isswapped = 0;
        if (isswapped != 0) {
            flags |= TH_FLAGS_SWAPPED;
        }
        int isidle = 0;
        if (isidle != 0) {
            flags |= TH_FLAGS_IDLE;
        }

        int32_t sleep_time = 0;
        int32_t curpri = 0;
        int32_t priority = 0;
        int32_t maxpriority = 0;
        int32_t name = 0;

        struct proc_threadinfo info = {
            .pth_user_time                  = user_time,        /* user run time */
            .pth_system_time                = system_time,      /* system run time */
            .pth_cpu_usage                  = cpu_usage,        /* scaled cpu usage percentage */
            .pth_policy                     = policy,           /* scheduling policy in effect */
            .pth_run_state                  = run_state,        /* run state */
            .pth_flags                      = flags,            /* various flags */
            .pth_sleep_time                 = sleep_time,       /* number of seconds that thread */
            .pth_curpri                     = curpri,           /* current priority */
            .pth_priority                   = priority,         /* priority */
            .pth_maxpriority                = maxpriority,      /* max priority*/
            .pth_name                       = {name},             /* thread name, if any */
        };

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
    if (p == NULL) {
        return ESRCH;
    }

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
            error = fill_vnodeinfo(vp, &info.pvip.vip_vi, 1);
            
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
    proc_rele(p);
    
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
        if ((error = file_socket(fd, &so)) == 0) {
            struct socket_fdinfo info;
            
            bzero(&info, sizeof(info));
            fill_fileinfo(fp, p, fd, &info.pfi);
            if ((error = fill_socketinfo(so, &info.psi)) == 0) {
                error = procfs_copy_data((char *)&info, sizeof(info), uio);
            }

            // Release the hold on the fileproc structure
            file_drop(fd);
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
        struct proc_fdinfo *buf;
        int count = vcount(p);
        struct filedesc *fdp = proc_fdlist(p, buf, count);
        procfs_fdlock_spin(p);
        for (int i = 0; i < fdp->fd_nfiles; i++) {
            struct fileproc *fp = fdp->fd_ofiles[i];
            if (fp != NULL && !(fdp->fd_ofileflags[i] & UF_RESERVED)) {
                size++;
            }
        }
        procfs_fdunlock(p);
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
static int
procfs_copy_data(char *data, int data_len, uio_t uio) {
    int error = 0;
    off_t start_offset = uio_offset(uio);
    data_len -= start_offset;
    if (data_len >= 0) {
        error = uiomove(data + start_offset, (int)data_len, uio);
    }
    return error;
}
