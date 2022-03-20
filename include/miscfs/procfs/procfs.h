//
//  procfs.h
//  ProcFS
//
//  Created by Kim Topley on 12/1/15.
//
// Definitions for the procfs file system. The content of this
// header is available to both user-level and kernel components.
//

#ifndef procfs_h
#define procfs_h

#pragma mark -
#pragma mark Common Definitions

// File system type name.
#define PROCFS_FSNAME        "procfs"
#define PROCFS_BUNDLEID      "com.stupid.filesystems.procfs"
#define PROCFS_BUILDNUM      "1"
#define PROCFS_VERSION       "1.0.0"
#define PROCFS_LCK_GRP_NAME  PROCFS_BUNDLEID ".lckgrp"
#define PROCFS_NOTYPENUM     0

// Mount option flags.
// Do not apply process permissions to the pid entries in /proc.
#define PROCFS_MOPT_NOPROCPERMS (1 << 0)

/*
 * The procfs mount structure, created by mount_procfs
 * and passed to the kernel by the mount(2) system call.
 */
typedef struct procfs_mount_args {
    int mnt_options;      // The procfs mount options.
} procfs_mount_args_t;

#pragma mark -
#pragma mark Internel Definitions - Kernel Only

#ifndef __FSBUNDLE__

#include <libkern/OSMalloc.h>
#include <libkext/libkext.h>
#include <mach/task.h>
#include <sys/kernel_types.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/vnode.h>

// Tag used for memory allocation.
extern OSMallocTag procfs_osmalloc_tag;

#pragma mark -
#pragma mark Type Definitions

typedef struct pfsnode pfsnode_t;
typedef struct pfsid pfsid_t;
typedef struct pfsmount pfsmount_t;
typedef struct pfssnode pfssnode_t;

// Callback function used to create vnodes, called from within the
// procfsnode_find() function. "params" is used to pass the details that
// the function needs in order to create the correct vnode. It is obtained
// from the "create_vnode_params" argument passed to procfsnode_find(),
// "pnp" is a pointer to the pfsnode_t that the vnode should be linked to
// and "vpp" is where the created vnode will be stored, if the call was successful.
// Returns 0 on success or an error code (from errno.h) if not.
typedef int (*create_vnode_func)(void *params, pfsnode_t *pnp, vnode_t *vpp);

// Type for the base node id field of a structure node.
typedef uint16_t pfsbaseid_t;

// Type of a function that reports the size for a procfs node.
typedef size_t (*procfs_node_size_fn)(pfsnode_t *pnp, kauth_cred_t creds);

// Type of a function that reads the data for a procfs node.
typedef int (*procfs_read_data_fn)(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);

#pragma mark -
#pragma mark Common Definitions

enum vtype;

/*
 * The different types of node in a procfs filesystem
 */
typedef enum {
    PFSroot,        /* the filesystem root */
    PFSproc,        /* a process-specific sub-directory */
    PFSthread,      /* the thread dir */
    PFSdir,         /* an ordinary directory */
    PFSfile,        /* a file */
    PFSdirthis,     /* representation of "." */
    PFSdirparent,   /* representation of ".." */
    PFScurproc,     /* symbolic link for curproc */
    PFSprocnamedir, /* directory for a process labeled with its command line */
    PFSfd,          /* a directory containing the processes open fd's */
    PFScpuinfo,     /* /proc/cpuinfo */
} pfstype;

// VFS flags
#define PROCFS_VFS_FLAGS  ( \
        VFS_TBL64BITREADY   | \
        VFS_TBLFSNODELOCK   | \
        VFS_TBLLOCALVOL     | \
        VFS_TBLNOTYPENUM    | \
        VFS_TBLNOMACLABEL   | \
        0                     \
)

// Bit values for the psn_flags field.
#define PSN_FLAG_PROCESS    (1 << 0)
#define PSN_FLAG_THREAD     (1 << 1)

// Special values for the nodeid_pid and nodeid_objectid fields.
#define PRNODE_NO_PID       ((int)-1)
#define PRNODE_NO_OBJECTID  ((uint64_t)0)

// Root node id value.
#define PROCFS_ROOT_NODE_BASE_ID ((pfsbaseid_t)1)

// Largest name of a structure node.
#define MAX_STRUCT_NODE_NAME_LEN 16

#pragma mark -
#pragma mark Structure Definitions

/*
 * Definitions for the data structures that determine the
 * layout of nodes in the procfs file system.
 * The layout is constructed by building a tree of
 * structures of type pfssnode_t. The layout
 * is the same for each file system instance and is created
 * when the first instance of the file system is mounted.
 */

/*
 * An entry in the procfs file system layout. All fields of this
 * structure are set on creation and do not change, so no locking
 * is required to access them.
 *
 * The psn_node_type field is the type of the structure node. These types
 * are mapped to the usual vnode types by the file system when getting
 * node attributes and are used during node lookup and other vnode operations.
 *
 * The psn_name field is the name that should be used for the node in the
 * file system. For nodes of type PROCFS_PROCDIR and PROCFS_PROCNAME_DIR,
 * the process id of the associated process is used and for PROCFS_THREADDIR, 
 * the associated thread's id is used.
 *
 * The psn_base_node_id field is a unique value that becomes part of the
 * full id of any pfsnode_t that is created from this structure node.
 * 
 * The PSN_FLAG_PROCESS and PSN_FLAG_THREAD flag values of a node are propagated
 * to all descendent nodes, so it is always possible to determine whether a
 * node is process- and/or thread-related just by examining the psn_flags
 * field of its pfssnode.
 */
struct pfssnode {
    pfstype                                 psn_node_type;
    char                                    psn_name[MAX_STRUCT_NODE_NAME_LEN];
    pfsbaseid_t                   psn_base_node_id;   // Base node id - unique.
    uint16_t                                psn_flags;          // Flags - PSN_XXX (see below)

    // Structure linkage. Immutable once set.
    struct pfssnode                       *psn_parent;                          // The parent node in the structure
    TAILQ_ENTRY(pfssnode)     psn_next;                            // Next sibling node within structure parent.
    TAILQ_HEAD(procfs_structure_children,  pfssnode) psn_children; // Children of this structure node.

    // --- Function hooks. Set to null to use the defaults.
    // The node's size value. This is the size value for the node itself.
    // For directory nodes, the sum of the size values of all of its children is
    // used as the actual size, so this value has meaning only for nodes of type
    // PFSfile. It is not used if the procfs_node_size_fn field is set.
    size_t                                  psn_node_size;

    // Gets the value for the node's size attribute. If NULL, psn_node_size
    // is used instead.
    procfs_node_size_fn                     psn_getsize_fn;

    // Reads the file content.
    procfs_read_data_fn                     psn_read_data_fn;
};

/*
 * Composite identifier for a node in the procfs file system.
 * There must only ever be one node for each unique identifier
 * in any given instance of the file system (i.e. per mount).
 */
struct pfsid {
    int                     nodeid_pid;         // The owning process, or PRNODE_NO_PID if not process-linked
    uint64_t                nodeid_objectid;    // The owning object within the process, or PRNODE_NO_OBJECTID if none.
    pfsbaseid_t   nodeid_base_id;     // The id of the structure node to which this node is linked.
};

/*
 * Procfs per-moount data structure. Typically, there is only one
 * instance of this file system, but the implementation does not
 * preclude multiple mounts.
 */
struct pfsmount {
    int32_t                 pmnt_id;            // A unique identifier for this mount. Shared by all nodes.
    int                     pmnt_flags;         // Flags, set from the mount command (PROCFS_MOPT_XXX).
    struct mount           *pmnt_mp;            // VFS-level mount structure.
    struct timespec         pmnt_mount_time;    // Time at which the file system was mounted.
};

/*
 * The filesystem-dependent vnode private data for procfs.
 * There is one insance of this structure for each active node.
 */
struct pfsnode {
    // Linkage for the node hash. Protected by the node hash lock.
    LIST_ENTRY(pfsnode)  node_hash;

    // Pointer to the associated vnode. Protected by the node hash lock.
    vnode_t                 node_vnode;

    // Records whether this node is currently being attached to a vnode.
    // Only one thread can be allowed to link the node to a vnode. If a
    // thread that wants to create a pfsnode and link it to a vnode
    // finds this field set to true, it must release the node hash lock
    // and wait until the field is reset to false, then check again whether
    // some or all of the work that it needed to do has been completed.
    // Protected by the node hash lock.
    boolean_t               node_attaching_vnode;

    // Records whether a thread is awaiting the outcome of vnode attachment.
    // Protected by the node hash lock.
    boolean_t               node_thread_waiting_attach;

    // node_mnt_id and node_id taken together uniquely identify a node. There
    // must only ever be one procnfsnode instance (and hence one vnode) for each
    // (node_mnt_id, node_id) combination. The node_mnt_id value can be obtained
    // from the pmnt_id field of the pfsmount structure for the owning mount.
    int32_t                 node_mnt_id;            // Identifier of the owning mount.
    pfsid_t         node_id;                // The identifer of this node.

    // Pointer to the pfssnode_t for this node.
    pfssnode_t *node_structure_node;   // Set when allocated, never changes.

    // Open flags
    u_long                  node_flags;
};

/*
 * Structure used to keep track of pid collection.
 */
struct procfs_pidlist_data
{
    int             num_procs;
    int             max_procs;
    kauth_cred_t    creds;     // Credential to use for access check, or NULL
    int            *pids;
};

#pragma mark -
#pragma mark Inline Conversion Functions

/* Convert from procfs vnode pointer to VFS vnode pointer. */
static inline vnode_t
pfsnode_to_vnode(pfsnode_t *pnp)
{
    return pnp->node_vnode;
}

/* Convert from VFS vnode pointer to procfs vnode pointer. */
static inline pfsnode_t *
vnode_to_procfsnode(vnode_t vp)
{
    return (pfsnode_t *)vnode_fsnode(vp);
}

/* Convert from procfs mount pointer to VFS mount pointer. */
static inline struct mount *
procfs_mp_to_vfs_mp(pfsmount_t *pmp)
{
    return pmp->pmnt_mp;
}

/* Convert from VFS mount pointer to procfs mount pointer. */
static inline pfsmount_t *
vfs_mp_to_procfs_mp(struct mount *vmp)
{
    return(pfsmount_t *)vfs_fsprivate(vmp);
}

#pragma mark -
#pragma mark Inline Convenience Functions

/* Returns whether access checks should apply to the vnodes on a given mount point. */
static inline boolean_t
procfs_should_access_check(pfsmount_t *pmp)
{
    return (pmp->pmnt_flags & PROCFS_MOPT_NOPROCPERMS) == 0;
}

/* Returns whether a given node type represents a directory. */
static inline boolean_t
procfs_is_directory_type(pfstype type)
{
    return type != PFSfile && type != PFScurproc;
}

/* Gets the pid_t for the process corresponding to a pfsnode_t. */
static inline int
procfsnode_to_pid(pfsnode_t *pfsnode)
{
    return pfsnode->node_id.nodeid_pid;
}

#pragma mark -
#pragma mark Global Definitions

/* Identifier for the root node of the file system. */
extern const pfsid_t PROCFS_ROOT_NODE_ID;

/* Public API */
extern void procfsnode_start_init(void);
extern void procfsnode_complete_init(void);
extern int procfsnode_find(pfsmount_t *pmp,
                           pfsid_t node_id,
                           pfssnode_t *snode,
                           pfsnode_t **pnpp, vnode_t *vnpp,
                           create_vnode_func create_vnode_func,
                           void *create_vnode_params);
extern void procfsnode_reclaim(vnode_t vp);
extern void procfs_get_parent_node_id(pfsnode_t *pnp, pfsid_t *idp);

/* Gets the root node of the file system structure. */
extern pfssnode_t *procfs_structure_root_node(void);

/*
 * Initializes the procfs structures. Should only be called
 * while mounting a file system.
 */
extern void procfs_structure_init(void);

/*
 * Frees the memory for the procfs structures. Should only be called
 * while unmounting the last instance of the file system.
*/
extern void procfs_structure_free(void);

/* 
 * Gets the vnode type that is appropriate for a given structure node type.
 * This function serves a similar purpose as 'procfs_allocvp' from NetBSD's
 * procfs_subr.c file.
 */
extern enum vtype vnode_type_for_structure_node_type(pfstype);

/*
 * Copies data from the local buffer "data" into the area described
 * by a uio structure. The first byte of "data" is assumed to 
 * correspond to a zero offset, so if the uio structure has its
 * uio_offset set to N, the first byte of data that will be copied
 * is at data[N].
 */
extern int procfs_copy_data(const char *data, int data_len, uio_t uio);

/* Functions that copy pfsnode_t data to a buffer described by a uio_t structure. */
extern int procfs_read_pid_data(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_ppid_data(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_pgid_data(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_sid_data(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_tty_data(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_proc_info(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_task_info(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_thread_info(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_fd_data(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_socket_data(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);

extern int procfs_docpuinfo(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx);

/* Functions that return the data size for a node. */
extern size_t procfs_get_node_size_attr(pfsnode_t *pnp, kauth_cred_t creds);
extern size_t procfs_process_node_size(pfsnode_t *pnp, kauth_cred_t creds);
extern size_t procfs_thread_node_size(pfsnode_t *pnp, kauth_cred_t creds);
extern size_t procfs_fd_node_size(pfsnode_t *pnp, kauth_cred_t creds);

/* Subroutine functions. */
extern boolean_t procfs_node_type_has_pid(pfstype node_type);
extern int procfs_get_process_info(vnode_t vp, pid_t *pidp, proc_t *procp);
extern uint64_t procfs_get_node_fileid(pfsnode_t *pnp);
extern uint64_t procfs_get_fileid(pid_t pid, uint64_t objectid, pfsbaseid_t base_id);
extern int procfs_atoi(const char *p, const char **end_ptr);
extern void procfs_get_pids(pid_t **pidpp, int *pid_count, uint32_t *sizep, kauth_cred_t creds);
extern void procfs_release_pids(pid_t *pidp, uint32_t size);
extern int procfs_get_thread_ids_for_task(task_t task, uint64_t **thread_ids, int *thread_count);
extern void procfs_release_thread_ids(uint64_t *thread_ids, int thread_count);
extern int procfs_check_can_access_process(kauth_cred_t creds, proc_t p);
extern int procfs_check_can_access_proc_pid(kauth_cred_t creds, pid_t pid);
extern int procfs_issuser(kauth_cred_t creds);
extern int procfs_get_process_count(kauth_cred_t creds);
extern int procfs_get_task_thread_count(task_t task);

#endif /* __FSBUNDLE__ */

#endif /* procfs_h */
