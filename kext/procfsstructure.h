//
//  procfsstructure.h
//  ProcFS
//
//  Created by Kim Topley on 12/26/15.
//
//

#ifndef procfsstructure_h
#define procfsstructure_h

#include <sys/kernel_types.h>
#include <sys/queue.h>
#include "procfs.h"

enum vtype;
typedef struct procfsnode procfsnode_t;

/*
 * Definitions for the data structures that determine the
 * layout of nodes in the procfs file system.
 * The layout is constructed by building a tree of
 * structures of type procfs_structure_node_t. The layout
 * is the same for each file system instance and is created
 * when the first instance of the file system is mounted.
 */

#pragma mark -
#pragma mark Structure Definitions

/*
 * Enumeration of the different types of structure node in the procfs
 * file system.
 */
typedef enum {
    PROCFS_ROOT = 0,        // The root node.
    PROCFS_PROCDIR,         // The directory for a process.
    PROCFS_THREADDIR,       // The directory for a thread.
    PROCFS_DIR,             // An ordinary directory.
    PROCFS_FILE,            // A file.
    PROCFS_DIR_THIS,        // Representation of ".".
    PROCFS_DIR_PARENT,      // Representation of "..".
    PROCFS_CURPROC,         // The symlink to the current process.
    PROCFS_PROCNAME_DIR,    // The directory for a process labeled with its command line
    PROCFS_FD_DIR,          // The directory for a file descriptor for a process.
} procfs_structure_node_type_t;

// Returns whether a given node type represents a directory.
static inline boolean_t procfs_is_directory_type(procfs_structure_node_type_t type) {
    return type != PROCFS_FILE && type != PROCFS_CURPROC;
}

// Type for the base node id field of a structure node.
typedef uint16_t procfs_base_node_id_t;

// Root node id value.
#define PROCFS_ROOT_NODE_BASE_ID ((procfs_base_node_id_t)1)

// Largest name of a structure node.
#define MAX_STRUCT_NODE_NAME_LEN 16

// Type of a function that reports the size for a procfs node.
typedef size_t (*procfs_node_size_fn)(procfsnode_t *pnp, kauth_cred_t creds);

// Type of a function that reads the data for a procfs node.
typedef int (*procfs_read_data_fn)(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);

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
 * full id of any procfsnode_t that is created from this structure node.
 * 
 * The PSN_FLAG_PROCESS and PSN_FLAG_THREAD flag values of a node are propagated
 * to all descendent nodes, so it is always possible to determine whether a
 * node is process- and/or thread-related just by examining the psn_flags
 * field of its procfs_structure_node.
 */
typedef struct procfs_structure_node {
    procfs_structure_node_type_t        psn_node_type;
    char                                psn_name[MAX_STRUCT_NODE_NAME_LEN];
    procfs_base_node_id_t               psn_base_node_id;   // Base node id - unique.
    uint16_t                            psn_flags;          // Flags - PSN_XXX (see below)
    
    // Structure linkage. Immutable once set.
    struct procfs_structure_node        *psn_parent;        // The parent node in the structure
    TAILQ_ENTRY(procfs_structure_node)   psn_next;          // Next sibling node within structure parent.
    TAILQ_HEAD(procfs_structure_children, procfs_structure_node) psn_children;
                                                            // Children of this structure node.
    
    // --- Function hooks. Set to null to use the defaults.
    // The node's size value. This is the size value for the node itself.
    // For directory nodes, the sum of the size values of all of its children is
    // used as the actual size, so this value has meaning only for nodes of type
    // PROCFS_FILE. It is not used if the procfs_node_size_fn field is set.
    size_t                              psn_node_size;

    // Gets the value for the node's size attribute. If NULL, psn_node_size
    // is used instead.
    procfs_node_size_fn                 psn_getsize_fn;
    
    // Reads the file content.
    procfs_read_data_fn                 psn_read_data_fn;
} procfs_structure_node_t;

// Bit values for the psn_flags field.
#define PSN_FLAG_PROCESS    (1 << 0)
#define PSN_FLAG_THREAD     (1 << 1)

#pragma mark -
#pragma mark Global Definitions

// Gets the root node of the file system structure.
extern procfs_structure_node_t *procfs_structure_root_node(void);

// Initializes the procfs structures. Should only be called
// while mounting a file system.
extern void procfs_structure_init(void);

// Frees the memory for the procfs structures. Should only be called
// while unmounting the last instance of the file system.
extern void procfs_structure_free(void);

// Gets the vnode type that is appropriate for a given structure node type.
extern enum vtype vnode_type_for_structure_node_type(procfs_structure_node_type_t);

#endif /* procfsstructure_h */
