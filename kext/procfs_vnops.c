//
//  procfs_vnops.c
//  ProcFS
//
//  Created by Kim Topley on 12/3/15.
//
//
#include <libkern/libkern.h>
#include <sys/dirent.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/filedesc.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include "procfs.h"
#include "procfsnode.h"
#include "procfs_data.h"
#include "procfs_subr.h"

#pragma mark -
#pragma mark Local Definitions

// Read and execute permissions for all users.
#define READ_EXECUTE_ALL (S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

// Read and permission for all users.
#define READ_ALL (S_IRUSR|S_IRGRP|S_IROTH)

// Read/write/execute permissions for owner, read/execute for group and owner.
#define RWX_OWNER_RX_ALL (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

// Read, write and execute permissions for owner and group only.
#define ALL_ACCESS_OWNER_GROUP_ONLY (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP)

// Read, write and execute permissions for everone.
#define ALL_ACCESS_ALL (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)

// Vnode Operations Function Descriptor
#define VOPFUNC int (*)(void *)

// Structure used to hold the values needed to create a new vnode
// corresponding to a procfsnode_t.
STATIC typedef struct {
    // Parent vnode.
    vnode_t vca_parentvp;
    
} procfs_vnode_create_args;

// Size of a buffer large enough to hold the string form of a pid_t
// plus the command name for a process and some padding
static const int PAD_SIZE = 8;
static const int PID_SIZE = 16;
static const int PROCESS_NAME_SIZE = MAXCOMLEN + PID_SIZE + PAD_SIZE;

#pragma mark -
#pragma mark External References

extern struct proclist allproc;

#pragma mark -
#pragma mark Function Prototypes

STATIC int procfs_vnop_lookup(struct vnop_lookup_args *ap);
STATIC int procfs_vnop_getattr(struct vnop_getattr_args *ap);
STATIC int procfs_vnop_reclaim(struct vnop_reclaim_args *ap);
STATIC int procfs_vnop_readdir(struct vnop_readdir_args *ap);
STATIC int procfs_vnop_readlink(struct vnop_readlink_args *ap);
STATIC int procfs_vnop_read(struct vnop_read_args *ap);
STATIC int procfs_vnop_open(struct vnop_open_args *ap);
STATIC int procfs_vnop_close(struct vnop_close_args *ap);
STATIC int procfs_vnop_access(struct vnop_access_args *ap);
STATIC int procfs_vnop_inactive(struct vnop_inactive_args *ap);

STATIC inline int procfs_calc_dirent_size(const char *name);
STATIC int procfs_copyout_dirent(int type, uint64_t file_id, const char *name, uio_t uio, int *sizep);
STATIC int procfs_create_vnode(procfs_vnode_create_args *cap, procfsnode_t *pnp, vnode_t *vpp);
STATIC void procfs_construct_process_dir_name(proc_t p, char *buffer);


// Entries for the vnode operations that this file system supports.
// This table is converted to a fully-populated vnode operations
// vector when procfs is registered as a file system and a pointer
// to that vector is stored in procfs_vnodeop_p.
struct vnodeopv_entry_desc procfs_vnodeop_entries[] = {
    { &vnop_default_desc,   (VOPFUNC)vn_default_error },
    { &vnop_lookup_desc,    (VOPFUNC)procfs_vnop_lookup },      /* lookup */
    { &vnop_create_desc,    (VOPFUNC)vn_default_error },        /* create */
    { &vnop_open_desc,      (VOPFUNC)procfs_vnop_open },        /* open */
    { &vnop_mknod_desc,     (VOPFUNC)vn_default_error },        /* mknod */
    { &vnop_close_desc,     (VOPFUNC)procfs_vnop_close },       /* close */
    { &vnop_access_desc,    (VOPFUNC)procfs_vnop_access },      /* access */
    { &vnop_getattr_desc,   (VOPFUNC)procfs_vnop_getattr },     /* getattr */
    { &vnop_setattr_desc,   (VOPFUNC)vn_default_error },        /* setattr */
    { &vnop_read_desc,      (VOPFUNC)procfs_vnop_read },        /* read */
    { &vnop_write_desc,     (VOPFUNC)vn_default_error },        /* write */
    { &vnop_ioctl_desc,     (VOPFUNC)vn_default_error },        /* ioctl */
    { &vnop_select_desc,    (VOPFUNC)vn_default_error },        /* select */
    { &vnop_mmap_desc,      (VOPFUNC)vn_default_error },        /* mmap */
    { &vnop_fsync_desc,     (VOPFUNC)vn_default_error },        /* fsync */
    { &vnop_remove_desc,    (VOPFUNC)vn_default_error },        /* remove */
    { &vnop_link_desc,      (VOPFUNC)vn_default_error },        /* link */
    { &vnop_rename_desc,    (VOPFUNC)vn_default_error },        /* rename */
    { &vnop_mkdir_desc,     (VOPFUNC)vn_default_error},         /* mkdir */
    { &vnop_rmdir_desc,     (VOPFUNC)vn_default_error },        /* rmdir */
    { &vnop_symlink_desc,   (VOPFUNC)vn_default_error },        /* symlink */
    { &vnop_readdir_desc,   (VOPFUNC)procfs_vnop_readdir },     /* readdir */
    { &vnop_readlink_desc,  (VOPFUNC)procfs_vnop_readlink },    /* readlink */
    { &vnop_inactive_desc,  (VOPFUNC)procfs_vnop_inactive },    /* inactive */
    { &vnop_reclaim_desc,   (VOPFUNC)procfs_vnop_reclaim },     /* reclaim */
    { &vnop_strategy_desc,  (VOPFUNC)vn_default_error },        /* strategy */
    { &vnop_pathconf_desc,  (VOPFUNC)vn_default_error },        /* pathconf */
    { &vnop_advlock_desc,   (VOPFUNC)vn_default_error },        /* advlock */
    { &vnop_bwrite_desc,    (VOPFUNC)vn_default_error },        /* bwrite */
    { &vnop_pagein_desc,    (VOPFUNC)vn_default_error },        /* Pagein */
    { &vnop_pageout_desc,   (VOPFUNC)vn_default_error },        /* Pageout */
    { &vnop_copyfile_desc,  (VOPFUNC)vn_default_error },        /* Copyfile */
    { &vnop_blktooff_desc,  (VOPFUNC)vn_default_error },        /* blktooff */
    { &vnop_offtoblk_desc,  (VOPFUNC)vn_default_error },        /* offtoblk */
    { &vnop_blockmap_desc,  (VOPFUNC)vn_default_error },        /* blockmap */
    { NULL,                 (VOPFUNC)NULL }
};

// Pointer to the constructed vnode operations vector. Set
// when the file system is registered and used when creating
// vnodes.
int (**procfs_vnodeop_p)(void *);

// Descriptor used to create the vnode operations vector for
// procfs from procfs_vnodeop_entries. Entries for operations that
// we do not support will get appropriate defaults.
struct vnodeopv_desc procfs_vnodeop_opv_desc = {
    &procfs_vnodeop_p, procfs_vnodeop_entries
};

// List of descriptors used to build vnode operations
// vectors. Since we only have one set of vnode operations,
// there is only one descriptor.
struct vnodeopv_desc *procfs_vnodeops_list[] = {
    &procfs_vnodeop_opv_desc,
    NULL
};

#pragma mark -
#pragma mark Vnode Operations

/*
 * Vnode operations that don't require us to do anything.
 */
STATIC int
procfs_vnop_open(__unused struct vnop_open_args *ap) {
    return 0;
}


STATIC int
procfs_vnop_access(__unused struct vnop_access_args *ap) {
    return 0;
}

STATIC
int procfs_vnop_close(__unused struct vnop_close_args *ap) {
    return 0;
}

STATIC
int procfs_vnop_inactive(__unused struct vnop_inactive_args *ap) {
    // We do everything in procfs_vnop_reclaim.
    return 0;
}

/*
 * Vnode lookup, called when resolving a path. Each invocation of this
 * function requires us to resolve one level of path name and return
 * either an error or the vnode that corresponds to it, with an
 * additional iocount that must eventually be released by the caller.
 *
 * When asked to resolve a path, we are given the vnode of the 
 * path's directory and the path segment. The vnode will map to a
 * procfsnode_t, which we can use to get its procfs_structure_node_t.
 * Once we have the procfs_structure_node_t, we know which level we 
 * are at in the file system and therefore which paths are valid.
 * In some cases, we can resolve the lookup by a simple comparison
 * of the path name with the name of a procfs_structure_node_t. As
 * an exmaple, if the parent node is for a process, then the node
 * structure tells us that names like "ppid", "pgid" etc are valid.
 * In other cases, we have to do more work. In the root directory,
 * for example, most of the valid names are process ids, so we have
 * to check whether the name component is numeric and whether it
 * corresponds to an active process.
 * 
 * The end result of the name check will be a procfs_structure_node_t.
 * From that, we can construct the node id of the node that the name
 * refers to and we can then use that to look up the vnode in the
 * vnode cache and create it if it's not there.
 */
STATIC int
procfs_vnop_lookup(struct vnop_lookup_args *ap) {
    char name[NAME_MAX + 1];
    int error = 0;
    struct componentname *cnp = ap->a_cnp;
    vnode_t dvp = ap->a_dvp; // Parent of the name to be looked up

    // The parent directory must not be NULL and the name
    // length must be at least 1.
    if (dvp == NULLVP || vnode_vtype(dvp) != VDIR || cnp->cn_namelen < 1) {
        error = EINVAL;
        goto out;
    }

    // Get the procfsnode_t for the directory. This must
    // not be NULL.
    procfsnode_t *dir_pnp = vnode_to_procfsnode(dvp);
    if (dir_pnp == NULL) {
        error = EINVAL;
        goto out;
    }

    // Preparation: get the component that we are looking up, clear
    // the returned vnode and ensure that nothing is added to the
    // name cache.
    strlcpy(name, cnp->cn_nameptr, min(sizeof(name), cnp->cn_namelen + 1));
    cnp->cn_flags &= ~MAKEENTRY;
    *ap->a_vpp = NULLVP;
    procfs_mount_t *mp = vfs_mp_to_procfs_mp(vnode_mount(dvp));      // procfs file system mount.
    
    if (cnp->cn_flags & ISDOTDOT) {
        // We need the parent of the directory in "dvp". Get that by figuring out what its
        // node id would be.
        procfsnode_id_t parent_node_id;
        procfs_get_parent_node_id(dir_pnp, &parent_node_id);
        procfsnode_t *target_procfsnode;
        vnode_t target_vnode;
        procfs_vnode_create_args create_args;
        create_args.vca_parentvp = NULLVP;
        error = procfsnode_find(mp, parent_node_id,
                                dir_pnp->node_structure_node->psn_parent,
                                &target_procfsnode,
                                &target_vnode,
                                (create_vnode_func)&procfs_create_vnode,
                                &create_args);
        if (error == 0) {
            *ap->a_vpp = target_vnode;
        }
    } else if (cnp->cn_namelen == 1 && name[0] == '.') {
        // Looking for the current directory, so return
        // "dvp" with an extra iocount reference.
        error = vnode_get(dvp);
        *ap->a_vpp = dvp;
    } else {
        // For all other cases, we try to match the name component
        // against the child nodes of the directory's structure node.
        // If we find a process or thread  structure node, we try to
        // convert the name to an integer and match if successful.
        procfs_structure_node_t *dir_snode = dir_pnp->node_structure_node;
        procfs_structure_node_t *match_node;
        procfsnode_id_t match_node_id;
        proc_t target_proc = NULL;
        TAILQ_FOREACH(match_node, &dir_snode->psn_children, psn_next) {
            assert(error == 0);
            procfs_structure_node_type_t node_type = match_node->psn_node_type;
            if (strcmp(name, match_node->psn_name) == 0) {
                // Name matched. This is the droid we are looking for. Construct the
                // node_id from the matched node and the pid and object id of the
                // parent directory.
                match_node_id.nodeid_base_id = match_node->psn_base_node_id;
                match_node_id.nodeid_pid = dir_pnp->node_id.nodeid_pid;
                match_node_id.nodeid_objectid = dir_pnp->node_id.nodeid_objectid;
                break;
            } else if (node_type == PROCFS_FD_DIR) {
                // Entries in this directory must be numeric and must correspond to
                // an open file descriptor in the process.
                const char *endp;
                int id = procfs_atoi(name, &endp);
                boolean_t valid = FALSE;
                if (id != -1) {
                    // Check whether it is a valid file descriptor.
                    target_proc = proc_find(dir_pnp->node_id.nodeid_pid);
                    if (target_proc != NULL) { // target_proc is released at loop end.
                        struct filedesc *fdp = target_proc->p_fd;
                        proc_fdlock_spin(target_proc);
                        if (id < fdp->fd_nfiles) {
                            struct fileproc *fp = fdp->fd_ofiles[id];
                            valid = fp!= NULL && !(fdp->fd_ofileflags[id] & UF_RESERVED);
                        }
                        proc_fdunlock(target_proc);
                    }
                }
                
                if (valid) {
                    // Construct the node id from the process id and file number.
                    match_node_id.nodeid_base_id = match_node->psn_base_node_id;
                    match_node_id.nodeid_pid = dir_pnp->node_id.nodeid_pid;
                    match_node_id.nodeid_objectid = id;
                } else {
                    error = ENOENT;
                }
                break;
            } else if (node_type == PROCFS_PROCDIR || node_type == PROCFS_PROCNAME_DIR
                       || node_type == PROCFS_THREADDIR) {
                // Process or thread directory entry marker. For PROCFS_PROCDIR and
                // PROCFS_THREADDIR, this can match only if "name" is a valid integer.
                // For PROCFS_PROCNAME_DIR, it has to be something like "1: launchd".
                const char *endp;
                int id = procfs_atoi(name, &endp);
                if (node_type != PROCFS_PROCNAME_DIR && *endp != (char)0) {
                    // Non-numeric before the end of the name -- this is invalid
                    // so skip this entry.
                    continue;
                }
                if (id != -1) {
                    // An integer, so this node is a potential match. Construct the node id
                    // from the base node id of the matched node and the parent directory
                    // node's pid and object id, replacing either the pid or the object id
                    // with the value constructed from the name being looked up.
                    match_node_id.nodeid_base_id = match_node->psn_base_node_id;
                    match_node_id.nodeid_pid = node_type ==
                            PROCFS_PROCDIR || node_type == PROCFS_PROCNAME_DIR ? id : dir_pnp->node_id.nodeid_pid;
                    match_node_id.nodeid_objectid = node_type == PROCFS_THREADDIR ? id : dir_pnp->node_id.nodeid_objectid;
                    
                    // The pid must match an existing process.
                    target_proc = proc_find(match_node_id.nodeid_pid);
                    if (target_proc == NULL) {
                        // No matching process.
                        error = ENOENT;
                        break;
                    }
                    
                    // For the case of PROCFS_PROCNAME_DIR, the name must be a complete
                    // and literal match to the full name that corresponds to the process
                    // id from the first part of the name.
                    if (node_type == PROCFS_PROCNAME_DIR) {
                        char name_buffer[PROCESS_NAME_SIZE];
                        procfs_construct_process_dir_name(target_proc, name_buffer);
                        if (strcmp(name, name_buffer) != 0) {
                            // Mismatched.
                            error = ENOENT;
                            break;
                        }
                    }
                    
                    // Determine whether an access check is required for access to
                    // the target process directory and its subdirectories. Do not
                    // check if root or if the file system is mounted with
                    // the "noprocperms" option.
                    boolean_t suser = vfs_context_suser(ap->a_context) == 0;
                    procfs_mount_t *pmp = vfs_mp_to_procfs_mp(vnode_mount(dvp));
                    boolean_t check_access = !suser && procfs_should_access_check(pmp);
                    kauth_cred_t creds = ap->a_context->vc_ucred;
                    if (check_access && procfs_check_can_access_process(creds, target_proc) != 0) {
                        // Access not permitted - claim that the path does not exist.
                        error = ENOENT;
                        break;
                    } else {
                        // If we have a thread id, it must match a thread of the process.
                        if (node_type == PROCFS_THREADDIR) {
                            uint64_t *thread_ids;
                            int thread_count;
                            
                            task_t task = proc_task(target_proc);
                            int result =  procfs_get_thread_ids_for_task(task, &thread_ids, &thread_count);
                            if (result == KERN_SUCCESS) {
                                boolean_t found = FALSE;
                                for (int i = 0; i < thread_count; i++) {
                                    if (thread_ids[i] == match_node_id.nodeid_objectid) {
                                        found = TRUE;
                                        break;
                                    }
                                }
                                procfs_release_thread_ids(thread_ids, thread_count);
                                
                                if (found == FALSE) {
                                    error = ENOENT;
                                    break;
                                }
                            } else {
                                error = ENOENT;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }

        if (target_proc != NULL) {
            proc_rele(target_proc);
        }
        
        // We have a match if match_node is not NULL.
        if (match_node != NULL && error == 0) {
            // We matched and match_node_id has been set to the node id of the
            // required node. Look for it in the cache, or create it if it is
            // not there. This also creates the vnode and increments its iocount.
            procfsnode_t *target_procfsnode;
            vnode_t target_vnode;
            procfs_vnode_create_args create_args;
            create_args.vca_parentvp = dvp;
            error = procfsnode_find(mp, match_node_id,
                                    match_node,
                                    &target_procfsnode,
                                    &target_vnode,
                                    (create_vnode_func)&procfs_create_vnode,
                                    &create_args);
            if (error == 0) {
                *ap->a_vpp = target_vnode;
            }
        } else if (error == 0) {
            // No match
            error = ENOENT;
        }
    }

out:
    return error;
}

/*
 * Implementation of the VNOP_READDIR operation. Given a directory vnode,
 * returns as many directory entries as will fit in the area described by
 * a uio structure.
 * 
 * The content of a directory depends on its type, which is obtained from
 * its procfs_structure_node_t object. In the simplest case, the directory
 * entries are simply the children of the structure node. This is the case
 * for process directories (e.g. /proc/1), for example, where the directory
 * entries are fixed vy the node structure. In the case of the root directory
 * (and several others) the content has to be determined dynamically based on
 * the running processes that are visible to the user. 
 *
 * Each directory entry is made as small as possible by only including the
 * non-null part of the file name. That means that the entries are of variable
 * size. To read a whole directory, the caller may need to invoke this operation
 * multiple times, each time with a different uio_offset value. Since it's not
 * possible to track which directory entry a given offset corrsponds to between
 * calls (especially since the logical content of the directory may change as 
 * a result of processes starting or terminating), the process always starts
 * from the first entry. However, entries are only copied out to the caller
 * once the uio_offset value has been reached.
 */
STATIC int
procfs_vnop_readdir(struct vnop_readdir_args *ap) {
    vnode_t vp = ap->a_vp;
    if (vnode_vtype(vp) != VDIR) {
        return ENOTDIR;
    }
    
    procfsnode_t *dir_pnp = vnode_to_procfsnode(vp);
    procfs_structure_node_t *dir_snode = dir_pnp->node_structure_node;
    
    int numentries = 0;
    int error = 0;
    uio_t uio = ap->a_uio;
    off_t nextpos = 0;
    off_t startpos = uio_offset(uio);
    
    // Determine whether access checks are required for process-related
    // nodes. Do not check if root or if the file system is mounted with
    // the "noprocperms" option.
    boolean_t suser = vfs_context_suser(ap->a_context) == 0;
    procfs_mount_t *pmp = vfs_mp_to_procfs_mp(vnode_mount(vp));
    boolean_t check_access = !suser && procfs_should_access_check(pmp);
    kauth_cred_t creds = ap->a_context->vc_ucred;
    
    procfs_structure_node_t *snode = TAILQ_FIRST(&dir_snode->psn_children);
    while (snode != NULL && uio_resid(uio) > 0) {
        // We inherit the parent directory's pid and thread id for
        // most cases. This is overridden only for entries of type
        // PROCFS_PROCDIR and PROCFS_THREADDIR.
        pid_t pid = dir_pnp->node_id.nodeid_pid;
        uint64_t objectid = dir_pnp->node_id.nodeid_objectid;
        procfs_base_node_id_t base_node_id = snode->psn_base_node_id;
        const char *name = snode->psn_name;
        
        // If there is a process id associated with this node, perform
        // an access check if required. Skip the entry if the user
        // does not have permission to see it.
        if (pid == PRNODE_NO_PID || !check_access || procfs_check_can_access_proc_pid(creds, pid) == 0) {
            boolean_t procdir = FALSE;
            boolean_t procnamedir = FALSE;
            boolean_t threaddir = FALSE;
            boolean_t fddir = FALSE;
            int type = VREG;
            switch (snode->psn_node_type) {
            case PROCFS_ROOT: // Indicates structure error - skip it.
                printf("procfs_vnop_readdir: ERROR: found PROCFS_ROOT\n");
                continue;
                    
           case PROCFS_DIR:
                type = DT_DIR;
                break;
                    
            case PROCFS_FILE:
                type = DT_REG;
                break;
                    
            case PROCFS_DIR_THIS:
                type = DT_DIR;
                    
                // We need to use the node id of the directory node for this case.
                pid = dir_pnp->node_id.nodeid_pid;
                objectid = dir_pnp->node_id.nodeid_objectid;
                base_node_id = dir_pnp->node_id.nodeid_base_id;
                break;
                    
            case PROCFS_DIR_PARENT:
                type = DT_DIR;
                    
                // We need to use the node id of the directory's parent node for this case.
                procfsnode_id_t parent_node_id;
                procfs_get_parent_node_id(dir_pnp, &parent_node_id);
                pid = parent_node_id.nodeid_pid;
                objectid = parent_node_id.nodeid_objectid;
                base_node_id = parent_node_id.nodeid_base_id;
                break;
                    
            case PROCFS_CURPROC:
                type = DT_LNK;
                break;
                
            // We handle these cases separately.
            case PROCFS_PROCDIR:
                procdir = TRUE;
                break;

            case PROCFS_PROCNAME_DIR:
                procnamedir = TRUE;
                break;
                    
            case PROCFS_THREADDIR:
                threaddir = TRUE;
                break;
                    
            case PROCFS_FD_DIR:
                fddir = TRUE;
                break;
            }
        
            if (procdir || procnamedir) {
                // An entry that represents the list of all processes.
                // Iterate over all active processes and write entries for those that
                // should appear after the start position, until we fill up the
                // space or run out of processes. We don't include any processes that the
                // caller does not have permission to access, unless the file system
                // is mounted with the noprocperms option or the user is root.
                char name_buffer[PROCESS_NAME_SIZE];
                int pid_count;
                uint32_t pid_list_size;
                pid_t *pid_list;
                procfs_get_pids(&pid_list, &pid_count, &pid_list_size, check_access ? creds : NULL);
                
                // Process each process in turn. We only get back process ids for the
                // processes that the caller has permission to access.
                for (int i = 0; i < pid_count; i++) {
                    pid_t this_pid = pid_list[i];
                    if (procdir) {
                        // Use the process id as the name.
                        snprintf(name_buffer, PROCESS_NAME_SIZE, "%d", this_pid);
                    } else {
                        // Use the process id plus process command line, to create a
                        // unqiue entry. Skip if the process has gone away.
                        proc_t p = proc_find(this_pid);
                        if (p == NULL) {
                            // Process disappeared.
                            continue;
                        }
                        procfs_construct_process_dir_name(p, name_buffer);
                        proc_rele(p);
                    }
                    int size = procfs_calc_dirent_size(name_buffer);
                    
                    // Copy out only if we are past the start offset.
                    if (nextpos >= startpos) {
                        error = procfs_copyout_dirent(VDIR, procfs_get_fileid(this_pid,
                                            PRNODE_NO_OBJECTID, base_node_id), name_buffer, uio, &size);
                        if (error != 0 || size == 0) {
                            break;
                        }
                        numentries++;
                    }
                    nextpos += size;
                }
                
                procfs_release_pids(pid_list, pid_list_size);
                break;   // Exit from the outer loop.
            } else if (threaddir) {
                // Iterate over all of the threads for the current process and write
                // entries for those that should appear after the start position,
                // until we fill up the space or run out of threads.
                proc_t p = proc_find(pid);
                if (p != NULL) {
                    task_t task = proc_task(p);
                    int thread_count;
                    uint64_t *thread_ids;
                    error = procfs_get_thread_ids_for_task(task, &thread_ids, &thread_count);
                    if (error == 0) {
                        char thread_buffer[PROCESS_NAME_SIZE];
                        for (int i = 0; i < thread_count; i++) {
                            uint64_t next_thread_id = thread_ids[i];
                            snprintf(thread_buffer, sizeof(thread_buffer), "%lld", next_thread_id);
                            int size = procfs_calc_dirent_size(thread_buffer);
                            // Copy out only if we are past the start offset.
                            if (nextpos >= startpos) {
                                error = procfs_copyout_dirent(VDIR, procfs_get_fileid(pid, next_thread_id, base_node_id), thread_buffer, uio, &size);
                                if (error != 0 || size == 0) {
                                    break;
                                }
                                numentries++;
                            }
                            nextpos += size;
                        }
                        procfs_release_thread_ids(thread_ids, thread_count);
                    }
                    proc_rele(p);
                    break;   // Exit from the outer loop.
                } else {
                    // No process for the current pid.
                    error = ENOENT;
                }
                
                if (error != 0) {
                    break;
                }
            } else if (fddir) {
                // Iterate over the open file descriptors for the current process
                // and write entries for those that should appear after the start position,
                // until we fill up the space or run out of threads.
                proc_t p = proc_find(pid);
                if (p != NULL) {
                    char fd_buffer[PROCESS_NAME_SIZE];
                    struct filedesc *fdp = p->p_fd;
                    for (int i = 0; i < fdp->fd_nfiles; i++) {
                        proc_fdlock_spin(p);
                        struct fileproc *fp = fdp->fd_ofiles[i];
                        if (fp != NULL && !(fdp->fd_ofileflags[i] & UF_RESERVED)) {
                            // Need to unlock before copy out in case of fault and because it's a "long" operation.
                            proc_fdunlock(p);
                            snprintf(fd_buffer, sizeof(fd_buffer), "%d", i);
                            int size = procfs_calc_dirent_size(fd_buffer);
                            
                            // Copy out only if we are past the start offset.
                            if (nextpos >= startpos) {
                                error = procfs_copyout_dirent(VDIR, procfs_get_fileid(pid, i, base_node_id), fd_buffer, uio, &size);
                                if (error != 0 || size == 0) {
                                    break;
                                }
                                numentries++;
                            }
                            nextpos += size;
                            proc_fdlock_spin(p);
                        }
                        proc_fdunlock(p);
                    }
                    proc_rele(p);
                    break;   // Exit from the outer loop.
                } else {
                    // No process for the current pid.
                    error = ENOENT;
                }
                
                if (error != 0) {
                    break;
                }
            } else {
                // Copy out only if we have reached the end offset
                // from the last call.
                if (nextpos >= startpos) {
                    int size = procfs_calc_dirent_size(name);
                    error = procfs_copyout_dirent(type, procfs_get_fileid(pid, objectid, base_node_id), name, uio, &size);
                    if (size == 0 || error != 0) {
                        // No room to copy out, or an error occurred - stop here.
                        break;
                    }
                    numentries++;
                    nextpos += size;
                }
            }
        }
        
        // Continue with the next node.
        snode = TAILQ_NEXT(snode, psn_next);
    }
    
    // Set output values for the next pass.
    uio_setoffset(uio, nextpos);
    *ap->a_eofflag = snode == NULL; // EOF if we handled the last entry
    *ap->a_numdirent = numentries;
    
    return error;
}

/*
 * Calculates the packed size for a directory entry for a given 
 * file name. The size is the sum of the fixed part of the dirent
 * structure plus the space required for the null-terminated name,
 * rounded up to a multiple of 4 bytes.
 */
STATIC int
procfs_calc_dirent_size(const char *name) {
    // We want to copy out a packed directory entry, which means we
    // need to calculate the actual length based on the length of the
    // name field, then round it to a 4-byte boundary.
    struct dirent entry;
    return (int)(sizeof(struct dirent) - sizeof(entry.d_name) + ((strlen(name) + 1 + 3) & ~3));
}


/*
 * Copies a directory entry out to the area described by a uio structure and updates
 * that structure. No copy is performed if there is not enough space to copy the entire
 * structure.
 */
STATIC int
procfs_copyout_dirent(int type, uint64_t file_id, const char *name, uio_t uio, int *sizep) {
    struct dirent entry;
    entry.d_type = type;
    entry.d_ino = (ino_t)file_id;
    entry.d_namlen = strlen(name);
    strlcpy(entry.d_name, name, entry.d_namlen + 1);
    
    int size = *sizep;
    entry.d_reclen = size;
    int error = 0;
    if (size <= uio_resid(uio)) {
        error = uiomove((const char * )&entry, (int)size, uio);
        *sizep = size;
    } else {
        // No room to copy out.
        *sizep = 0;
    }
    return error;
}

/*
 * Gets the attributes for a node, as seen by the stat(2) system
 * call. Many attributes don't make sense for procfs nodes, so are
 * not set. In other cases, the values are fixed. 
 *
 * The node permissions depend on whether the file system was mounted
 * with the "noprocperms" option. If it was, full access to all users
 * is granted. Otherwise, only owner and group have access, except for
 * symbolic links which always have mode 0777, allowing the access
 * decision to be made when resolving the target.
 */
STATIC int
procfs_vnop_getattr(struct vnop_getattr_args *ap) {
    vnode_t vp = ap->a_vp;
    procfsnode_t *procfs_node = vnode_to_procfsnode(vp);
    procfs_structure_node_t *snode = procfs_node->node_structure_node;
    procfs_structure_node_type_t node_type = snode->psn_node_type;
    
    pid_t pid;  // pid of the process for this node.
    proc_t p;   // proc_t for the process - NULL for the root node.
    
    // Get the process pid and proc_t for the target vnode.
    // Returns ENOENT if the process does not exist. For the
    // root vnode, p is zero and pid is PRNODE_NO_PID, but the
    // return value is zero.
    int error = procfs_get_process_info(vp, &pid, &p);
    if (error != 0) {
        return error;
    }
    
    // Permissions usually allow access only for the node's owning process and group,
    // but the "noprocperms" mount option can be used to allow read and execute access
    // to all users, if required. We reflect this by setting "modemask" to limit the
    // permissions that will be returned.
    procfs_mount_t *pmp = vfs_mp_to_procfs_mp(vnode_mount(vp));
    mode_t modemask = (pmp->pmnt_flags & PROCFS_MOPT_NOPROCPERMS) ? RWX_OWNER_RX_ALL : ALL_ACCESS_OWNER_GROUP_ONLY;
    
    struct vnode_attr *vap = ap->a_vap;
    switch (node_type) {
    case PROCFS_ROOT:
        // Root directory is accessible to everyone.
        VATTR_RETURN(vap, va_mode, READ_EXECUTE_ALL);
        break;
        
    case PROCFS_PROCDIR:
        VATTR_RETURN(vap, va_mode, READ_EXECUTE_ALL & modemask);
        break;
        
    case PROCFS_THREADDIR:
        VATTR_RETURN(vap, va_mode, READ_EXECUTE_ALL & modemask);
        break;
        
    case PROCFS_DIR:
        VATTR_RETURN(vap, va_mode, READ_EXECUTE_ALL & modemask);
        break;
        
    case PROCFS_FILE:
        VATTR_RETURN(vap, va_mode, READ_EXECUTE_ALL & modemask);
        break;
        
    case PROCFS_DIR_THIS:
        VATTR_RETURN(vap, va_mode, READ_EXECUTE_ALL & modemask);
        break;
        
    case PROCFS_DIR_PARENT:
        VATTR_RETURN(vap, va_mode, READ_EXECUTE_ALL & modemask);
        break;
            
    case PROCFS_FD_DIR:
        VATTR_RETURN(vap, va_mode, READ_EXECUTE_ALL);
        break;
        
    case PROCFS_CURPROC:        // Symbolic link to the calling process (FALLTHRU)
    case PROCFS_PROCNAME_DIR:   // Symbolic link to a process directory
        VATTR_RETURN(vap, va_mode, ALL_ACCESS_ALL);   // All access - target will determine actual access.
        break;
    }
    
    // ----- Generic attributes.
    VATTR_RETURN(vap, va_type, vnode_type_for_structure_node_type(node_type)); // File type
    VATTR_RETURN(vap, va_fsid, pmp->pmnt_id);                           // File system id.
    VATTR_RETURN(vap, va_fileid, procfs_get_node_fileid(procfs_node));  // Unique file id.
    VATTR_RETURN(vap, va_data_size,
                 procfs_get_node_size_attr(procfs_node, ap->a_context->vc_ucred)); // File size.
    
    // Use the process start time as the create time if we have a process.
    // otherwise use the file system mount time. Set the other times to the
    // same value, since there is really no way to track them.
    struct timespec create_time;
    if (p != NULL) {
        create_time.tv_sec = p->p_start.tv_sec;
        create_time.tv_nsec = p->p_start.tv_usec * 1000;
    } else {
        create_time.tv_sec = pmp->pmnt_mount_time.tv_sec;
        create_time.tv_nsec = pmp->pmnt_mount_time.tv_nsec;
    }
    VATTR_RETURN(vap, va_access_time, create_time);
    VATTR_RETURN(vap, va_change_time, create_time);
    VATTR_RETURN(vap, va_create_time, create_time);
    VATTR_RETURN(vap, va_modify_time, create_time);
    
    // Set the UID/GID from the credentials of the process that
    // corresponds to the procfsnode_t, if there is one. There
    // is no process for the root node. For other nodes. the uid
    // and gid are the real ids for the current process.
    proc_t current = current_proc();
    uid_t uid = current == NULL ? (uid_t)0 : current->p_ruid;
    gid_t gid = current == NULL ? (gid_t)0 : current->p_gid;
    if (p != NULL) {
        // Get the effective uid and gid from the process.
        uid = p->p_uid;
        gid = p->p_gid;
        
        proc_rele(p);
    }
    VATTR_RETURN(vap, va_uid, uid);
    VATTR_RETURN(vap, va_gid, gid);
    
    return error;
}

/*
 * Reads the content of a symbolic link. Only the "curproc" entry
 * and nodes in the "byname" directory are symbolic links. The
 * specific data for each case is generated and returned here.
 */
STATIC int
procfs_vnop_readlink(struct vnop_readlink_args *ap) {
    int error = 0;
    vnode_t vp = ap->a_vp;
    procfsnode_t *pnp = vnode_to_procfsnode(vp);
    procfs_structure_node_t *snode = pnp->node_structure_node;
    if (snode->psn_node_type == PROCFS_CURPROC) {
        // The link is "curproc". Get the pid of the current process
        // and copy it out to the caller's buffer.
        char pid_buffer[PROCESS_NAME_SIZE];
        proc_t p = current_proc();
        pid_t pid = p->p_pid;
        snprintf(pid_buffer, PROCESS_NAME_SIZE, "%d", pid);
        error = uiomove(pid_buffer, (int)strlen(pid_buffer), ap->a_uio);
    } else if (snode->psn_node_type == PROCFS_PROCNAME_DIR) {
        // A link from the process name to the process id. Create a target
        // of the form "../123" and copy it out to the caller's buffer.
        pid_t pid = pnp->node_id.nodeid_pid;
        char pid_buffer[PROCESS_NAME_SIZE];
        snprintf(pid_buffer, PROCESS_NAME_SIZE, "../%d", pid);
        error = uiomove(pid_buffer, (int)strlen(pid_buffer), ap->a_uio);
    } else {
        // Not valid for other node types.
        error = EINVAL;
    }
    return error;
}

/*
 * Reads a node's data. The read operation is delegated to a function
 * that's held in the node's procfs_structure_node_t. For nodes that
 * can't be read, the funtion is NULL and EINVAL will be returned,
 * except in the case of a directory, for which the error is EISDIR.
 */
STATIC int
procfs_vnop_read(struct vnop_read_args *ap) {
    vnode_t vp = ap->a_vp;
    procfsnode_t *pnp = vnode_to_procfsnode(vp);
    procfs_structure_node_t *snode = pnp->node_structure_node;
    procfs_read_data_fn read_data_fn = snode->psn_read_data_fn;
    
    int error = EINVAL;
    if (procfs_is_directory_type(snode->psn_node_type)) {
        error = EISDIR;
    } else if (read_data_fn != NULL) {
        error = read_data_fn(pnp, ap->a_uio, ap->a_context);
    }
    return error;
}

/**
 * Reclaims a vnode and its associated procfsnode_t when it's
 * no longer needed by the kernel file system code.
 */
STATIC int
procfs_vnop_reclaim(struct vnop_reclaim_args *ap) {
    procfsnode_reclaim(ap->a_vp);
    return 0;
}

#pragma mark -
#pragma mark Helper Functions

/*
 * Creates a vnode with given properties, which depend on the vnode type.
 */
STATIC int
procfs_create_vnode(procfs_vnode_create_args *cap, procfsnode_t *pnp, vnode_t *vpp) {
    procfs_structure_node_t *snode = pnp->node_structure_node;
    struct vnode_fsparam vnode_create_params;
    
    memset(&vnode_create_params, 0, sizeof(vnode_create_params));
    vnode_create_params.vnfs_mp = vnode_mount(cap->vca_parentvp);
    vnode_create_params.vnfs_vtype = vnode_type_for_structure_node_type(snode->psn_node_type);
    vnode_create_params.vnfs_str = "procfs vnode";
    vnode_create_params.vnfs_dvp = cap->vca_parentvp;
    vnode_create_params.vnfs_fsnode = pnp;
    vnode_create_params.vnfs_vops = procfs_vnodeop_p;
    vnode_create_params.vnfs_markroot = 0;
    vnode_create_params.vnfs_flags = VNFS_CANTCACHE;
    
    // Create the vnode, if possible.
    vnode_t new_vnode;
    int error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vnode_create_params, &new_vnode);
    
    // Return the root vnode pointer to the caller, if it was created.
    *vpp = error == 0 ? new_vnode : NULLVP;
    
    return error;
}

/*
 * Constructs the name of the directory for a given process. This
 * is simply a matter of converting its process id to a decimal
 * string.
 */
STATIC void
procfs_construct_process_dir_name(proc_t p, char *buffer) {
    pid_t pid = p->p_pid;
    int len = snprintf(buffer, PROCESS_NAME_SIZE, "%d ", pid);
    strlcpy(buffer + len, p->p_comm, MAXCOMLEN + 1);
}

