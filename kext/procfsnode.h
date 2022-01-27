//
//  procfsnode.h
//  ProcFS
//
//  Created by Kim Topley on 12/7/15.
//
//

#ifndef procfsnode_h
#define procfsnode_h

#if KERNEL 

#include <sys/vnode.h>
#include "procfsstructure.h"

#pragma mark -
#pragma mark File System Dependent Node for procfs

/*
 * Composite identifier for a node in the procfs file system.
 * There must only ever be one node for each unique identifier
 * in any given instance of the file system (i.e. per mount).
 */
typedef struct {
    pid_t                   nodeid_pid;         // The owning process, or PRNODE_NO_PID if not process-linked
    uint64_t                nodeid_objectid;    // The owning object within the process, or PRNODE_NO_OBJECTID if none.
    procfs_base_node_id_t   nodeid_base_id;     // The id of the structure node to which this node is linked.
} procfsnode_id_t;

// Special values for the nodeid_pid and nodeid_objectid fields.
#define PRNODE_NO_PID       ((pid_t)-1)
#define PRNODE_NO_OBJECTID  ((uint64_t)0)

/*
 * The filesystem-dependent vnode private data for procfs.
 * There is one insance of this structure for each active node.
 */
typedef struct procfsnode {
    // Linkage for the node hash. Protected by the node hash lock.
    LIST_ENTRY(procfsnode)  node_hash;
    
    // Pointer to the associated vnode. Protected by the node hash lock.
    vnode_t                 node_vnode;
    
    // Records whether this node is currently being attached to a vnode.
    // Only one thread can be allowed to link the node to a vnode. If a
    // thread that wants to create a procfsnode and link it to a vnode
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
    // from the pmnt_id field of the procfs_mount structure for the owning mount.
    int32_t                 node_mnt_id;            // Identifier of the owning mount.
    procfsnode_id_t         node_id;                // The identifer of this node.

    // Pointer to the procfs_structure_node_t for this node.
    procfs_structure_node_t *node_structure_node;   // Set when allocated, never changes.
} procfsnode_t;

#pragma mark -
#pragma mark Vnode to/from procfsnode Conversion

static inline vnode_t procfsnode_to_vnode(procfsnode_t *pnp) {
    return pnp->node_vnode;
}

static inline procfsnode_t *vnode_to_procfsnode(vnode_t vp) {
    return (procfsnode_t *)vnode_fsnode(vp);
}

#pragma mark -
#pragma mark Inline Convenience Functions

// Gets the pid_t for the process corresponding to a procfsnode_t
static inline pid_t procfsnode_to_pid(procfsnode_t *procfsnode) {
    return procfsnode->node_id.nodeid_pid;
}

#pragma mark -
#pragma mark Global Definitions

// Identifier for the root node of the file system.
extern const procfsnode_id_t PROCFS_ROOT_NODE_ID;

// Callback function used to create vnodes, called from within the
// procfsnode_find() function. "params" is used to pass the details that
// the function needs in order to create the correct vnode. It is obtained
// from the "create_vnode_params" argument passed to procfsnode_find(),
// "pnp" is a pointer to the procfsnode_t that the vnode should be linked to
// and "vpp" is where the created vnode will be stored, if the call was successful.
// Returns 0 on success or an error code (from errno.h) if not.
typedef int (*create_vnode_func)(void *params, procfsnode_t *pnp, vnode_t *vpp);

// Public API
extern void procfsnode_start_init(void);
extern void procfsnode_complete_init(void);
extern int procfsnode_find(procfs_mount_t *pmp,
                           procfsnode_id_t node_id,
                           procfs_structure_node_t *snode,
                           procfsnode_t **pnpp, vnode_t *vnpp,
                           create_vnode_func create_vnode_func,
                           void *create_vnode_params);
extern void procfsnode_reclaim(vnode_t vp);
extern void procfs_get_parent_node_id(procfsnode_t *pnp, procfsnode_id_t *idp);

#endif /* KERNEL */

#endif /* procfsnode_h */
