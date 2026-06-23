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

    if (p == PROC_NULL) {
        return ESRCH;
    }

    struct vnode_fdinfowithpath info;
    bzero(&info, sizeof(info));

    // Validate the descriptor and capture the vnode + vnode id + file info
    // under proc_fdlock (which keeps the fileproc alive). No fileproc iocount
    // is taken: the os_ref retain/release path is unavailable to a third-party
    // kext, so instead we re-acquire the vnode by id once the lock is dropped.
    vnode_t vp = NULLVP;
    uint32_t vid = 0;
    error = procfs_fd_vnode_info(p, fd, &vp, &vid, &info.pfi);
    if (error == 0) {
        // vnode_getwithvid() takes an iocount and fails cleanly if the vnode
        // was reclaimed between dropping proc_fdlock and here.
        error = vnode_getwithvid(vp, vid);
        if (error == 0) {
            error = fill_vnodeinfo(vp, &info.pvip.vip_vi, FALSE);
            if (error == 0) {
                // Add the file path and copy the data out to user space.
                int count = MAXPATHLEN;
                vn_getpath(vp, info.pvip.vip_path, &count);
                info.pvip.vip_path[MAXPATHLEN-1] = 0;
                // vn_getpath()/build_path() constructs the path backwards from
                // the end of the buffer and leaves a stale copy in the tail;
                // zero everything past the returned string so the node holds
                // the path exactly once.
                size_t plen = strlen(info.pvip.vip_path);
                bzero(info.pvip.vip_path + plen, MAXPATHLEN - plen);
                error = procfs_copy_data((const char *)&info, sizeof(info), uio);
            }
            vnode_put(vp);
        }
    }

    proc_rele(p);
    return error;
}

/*
 * Fills a socket_info from a socket using only the public, layout-independent
 * sock_* KPIs (the private fill_socketinfo() can neither be linked nor safely
 * forward-ported through the deep socket/inpcb/tcpcb structs). This covers the
 * common fields plus the addresses for IP and UNIX sockets; the per-protocol
 * deep state (e.g. TCP window/state) is left zeroed.
 */
static void
procfs_fill_socketinfo(socket_t so, struct socket_info *si)
{
    int dom = 0, type = 0, proto = 0;

    sock_gettype(so, &dom, &type, &proto);
    si->soi_so = (uint64_t)so;
    si->soi_type = type;
    si->soi_protocol = proto;
    si->soi_family = dom;

    if (dom == AF_INET || dom == AF_INET6) {
        struct in_sockinfo *ini;
        if (proto == IPPROTO_TCP) {
            si->soi_kind = SOCKINFO_TCP;
            ini = &si->soi_proto.pri_tcp.tcpsi_ini;
        } else {
            si->soi_kind = SOCKINFO_IN;
            ini = &si->soi_proto.pri_in;
        }

        struct sockaddr_storage ss;
        if (sock_getsockname(so, (struct sockaddr *)&ss, sizeof(ss)) == 0) {
            if (dom == AF_INET) {
                struct sockaddr_in *s = (struct sockaddr_in *)&ss;
                ini->insi_vflag = INI_IPV4;
                ini->insi_lport = s->sin_port;
                ini->insi_laddr.ina_46.i46a_addr4 = s->sin_addr;
            } else {
                struct sockaddr_in6 *s = (struct sockaddr_in6 *)&ss;
                ini->insi_vflag = INI_IPV6;
                ini->insi_lport = s->sin6_port;
                ini->insi_laddr.ina_6 = s->sin6_addr;
            }
        }
        if (sock_getpeername(so, (struct sockaddr *)&ss, sizeof(ss)) == 0) {
            if (dom == AF_INET) {
                struct sockaddr_in *s = (struct sockaddr_in *)&ss;
                ini->insi_fport = s->sin_port;
                ini->insi_faddr.ina_46.i46a_addr4 = s->sin_addr;
            } else {
                struct sockaddr_in6 *s = (struct sockaddr_in6 *)&ss;
                ini->insi_fport = s->sin6_port;
                ini->insi_faddr.ina_6 = s->sin6_addr;
            }
        }
    } else if (dom == AF_UNIX) {
        struct un_sockinfo *un = &si->soi_proto.pri_un;
        si->soi_kind = SOCKINFO_UN;
        sock_getsockname(so, (struct sockaddr *)&un->unsi_addr.ua_sun,
            sizeof(un->unsi_addr.ua_sun));
        sock_getpeername(so, (struct sockaddr *)&un->unsi_caddr.ua_sun,
            sizeof(un->unsi_caddr.ua_sun));
    } else {
        si->soi_kind = SOCKINFO_GENERIC;
    }
}

/*
 * Reads the data associated with a file descriptor that refers to a socket.
 */
int
procfs_read_socket_data(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;
    int fd = pnp->node_id.nodeid_objectid;
    proc_t p = proc_find(pnp->node_id.nodeid_pid);

    if (p == PROC_NULL) {
        return ESRCH;
    }

    struct socket_fdinfo info;
    bzero(&info, sizeof(info));
    socket_t so = NULL;

    // Validate the socket descriptor and take a sock_retain() reference under
    // proc_fdlock so the socket survives once the lock is dropped (sockets have
    // no vnode-id reclaim guard). info.pfi is filled while the lock is held.
    error = procfs_fd_socket(p, fd, &so, &info.pfi);
    if (error == 0) {
        procfs_fill_socketinfo(so, &info.psi);
        sock_release(so);
        error = procfs_copy_data((const char *)&info, sizeof(info), uio);
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

/*
 * Gets the size for the node that represents the file descriptors 
 * of a process. Counts one for every open file in the process.
 */
size_t
procfs_fd_node_size(pfsnode_t *pnp, __unused kauth_cred_t creds)
{
    size_t count = 0;

    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);

    if (p != PROC_NULL) {
        struct proc_fdinfo *fdlist = NULL;
        size_t fd_count = 0;
        if (procfs_get_fd_list(p, &fdlist, &fd_count) == 0) {
            count = fd_count;
        }
        procfs_release_fd_list(fdlist);
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
