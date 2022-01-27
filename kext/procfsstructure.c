//
//  procfsstructure.c
//  ProcFS
//
//  Created by Kim Topley on 12/26/15.
//
//
#include <kern/assert.h>
#include <kern/debug.h>
#include <libkern/OSMalloc.h>
#include <mach/boolean.h>
#include <sys/proc_info.h>
#include <sys/vnode.h>
#include <string.h>
#include "procfs_data.h"
#include "procfsstructure.h"

/*
 * Definition and management of the file system layout. The
 * layout is defined by a tree of procfs_structure_node_t
 * objects, starting with the root of the file system. The
 * structure is created in the procfs_structure_init() function
 * and is used while servicing VNOP_LOOKUP and VNOP_READDIR. To
 * add new file system nodes, add the corresponding entries in
 * procfs_structure_init() and make any necessary changes in
 * the procfs_vnop_lookup() and procfs_vnop_readdir() functions.
 * When adding files, it's also necessary to add functions that
 * return the file's data and its size, unless the size is
 * fixed. To do that, add the required functions in the
 * file procfs_data.c and link to them from the procfs_structure_node_t.
 */
#pragma mark -
#pragma mark Function Prototypes

STATIC procfs_structure_node_t *add_node(procfs_structure_node_t *parent,
                                         const char *name,
                                         procfs_structure_node_type_t type,
                                         procfs_base_node_id_t node_id,
                                         uint16_t flags,
                                         size_t size,
                                         procfs_node_size_fn node_size_fn,
                                         procfs_read_data_fn node_read_data_fn);
STATIC procfs_structure_node_t *add_directory(procfs_structure_node_t *parent,
                                         const char *name,
                                         procfs_structure_node_type_t type,
                                         procfs_base_node_id_t node_id,
                                         uint16_t flags,
                                         boolean_t raw,
                                         procfs_node_size_fn node_size_fn,
                                         procfs_read_data_fn node_read_data_fn);
STATIC procfs_structure_node_t *add_file(procfs_structure_node_t *parent,
                                         const char *name,
                                         procfs_base_node_id_t node_id,
                                         uint16_t flags,
                                         size_t size,
                                         procfs_node_size_fn node_size_fn,
                                         procfs_read_data_fn node_read_data_fn);
STATIC void release_node(procfs_structure_node_t *node);

// Next node id. No need to lock this value because access
// is guaranteed to be single-threaded. Start at 2 because the
// root node is always 1.
STATIC uint16_t next_node_id = PROCFS_ROOT_NODE_BASE_ID + 1;

// The root of the file system structure.
static procfs_structure_node_t *root_node;

#pragma mark - 
#pragma mark Externally Visible Functions

// Gets the root node of the file system structure.
procfs_structure_node_t *
procfs_structure_root_node(void) {
    return root_node;
}

// Initializes the procfs structures. Should only be called
// while mounting a file system. Given that restriction, we do
// not need to lock access to the structure data when deciding
// whether to creating it.
//
// NOTE: it is essential that the entries that expand to dynamic
// content be the last in their parent's child list. This makes
// the code that implements the readdir operation as simple as
// possible.
void
procfs_structure_init(void) {
    // Only do this on first mount.
    if (root_node == NULL) {
        // The root directory of the file system. This happens to be the only node
        // that has the same node id on all instance of this file system.
        root_node = add_directory(NULL, "/", PROCFS_ROOT, PROCFS_ROOT_NODE_BASE_ID, 0, 0, NULL, NULL);
        
        // A link in the root node to the current process entry. This will become a symbolic link.
        add_node(root_node, "curproc", PROCFS_CURPROC, next_node_id++, 0, 0, NULL, NULL);
        
        // A directory that contains all of the visible processes, listed by command name.
        // Each entry in this directory is a symbolic link to the process entry in root (e.g. "../123).
        procfs_structure_node_t *proc_by_name_dir = add_directory(root_node, "byname",
                        PROCFS_DIR, next_node_id++, 0, 0, NULL, NULL);
        
        // A pseudo-entry below "byname" that is replaced by nodes for all of the visible processes.
        // NOTE: this must be the last child entry for the "byname" node.
        add_directory(proc_by_name_dir, "__Process_N__",
                      PROCFS_PROCNAME_DIR, next_node_id++, PSN_FLAG_PROCESS, 0, procfs_process_node_size, NULL);
       
        // A pseudo-entry below "/" that is replaced by nodes for all of the visible processes.
        // NOTE: this must be the last child entry for the root node.
        procfs_structure_node_t *one_proc_dir = add_directory(root_node, "__Process__",
                       PROCFS_PROCDIR, next_node_id++, PSN_FLAG_PROCESS, 0, procfs_process_node_size, NULL);
        
        // A directory below the node for a process to hold all the file descriptors for that process.
        procfs_structure_node_t *fd_dir = add_directory(one_proc_dir, "fd",
                       PROCFS_DIR, next_node_id++, PSN_FLAG_PROCESS, 0, NULL, NULL);
        
        // A pseudo-entry below the "fd" node that is replaced by nodes for all the open files of
        // the current process.
        // NOTE: this must be the last child entry for the "fd" node.
        procfs_structure_node_t *one_fd_dir = add_directory(fd_dir, "__File__",
                      PROCFS_FD_DIR, next_node_id++, PSN_FLAG_PROCESS, 0, procfs_fd_node_size, NULL);
      
        // A directory below the node for a process to hold all the threads for that process.
        procfs_structure_node_t *threads_dir = add_directory(one_proc_dir, "threads",
                       PROCFS_DIR, next_node_id++, PSN_FLAG_PROCESS, 0, NULL, NULL);
        
        // A pseudo-entry below the "threads" node that is replaced by nodes for all the threads of
        // the current process.
        // NOTE: this must be the last child entry for the threads node.
        procfs_structure_node_t *one_thread_dir = add_directory(threads_dir, "__Thread__",
                      PROCFS_THREADDIR, next_node_id++, PSN_FLAG_PROCESS | PSN_FLAG_THREAD, 0, procfs_thread_node_size, NULL);
        
        // --- Per-proccess sub-directories and files.
        
        // Files that returns the process's pid, parent pid, process group id,
        // session id and controlling terminal name.
        add_file(one_proc_dir, "pid", next_node_id++, PSN_FLAG_PROCESS, sizeof(pid_t), NULL, procfs_read_pid_data);
        add_file(one_proc_dir, "ppid", next_node_id++, PSN_FLAG_PROCESS, sizeof(pid_t), NULL, procfs_read_ppid_data);
        add_file(one_proc_dir, "pgid", next_node_id++, PSN_FLAG_PROCESS, sizeof(pid_t), NULL, procfs_read_pgid_data);
        add_file(one_proc_dir, "sid", next_node_id++, PSN_FLAG_PROCESS, sizeof(pid_t), NULL, procfs_read_sid_data);
        add_file(one_proc_dir, "tty", next_node_id++, PSN_FLAG_PROCESS, 0, NULL, procfs_read_tty_data);
        add_file(one_proc_dir, "info", next_node_id++, PSN_FLAG_PROCESS, sizeof(struct proc_bsdinfo), NULL, procfs_read_proc_info);
        add_file(one_proc_dir, "taskinfo", next_node_id++, PSN_FLAG_PROCESS, sizeof(struct proc_taskinfo), NULL, procfs_read_task_info);
        
        // --- Per thread files.
        add_file(one_thread_dir, "info", next_node_id++, PSN_FLAG_PROCESS | PSN_FLAG_THREAD, sizeof(struct proc_taskinfo), NULL, procfs_read_thread_info);
        
        // --- Per file descriptor files.
        add_file(one_fd_dir, "details", next_node_id++, PSN_FLAG_PROCESS, sizeof(struct proc_threadinfo), NULL, procfs_read_fd_data);
        add_file(one_fd_dir, "socket", next_node_id++, PSN_FLAG_PROCESS, 0, NULL, procfs_read_socket_data);
    }
}

// Frees the memory for the procfs structures. Should only be called
// while unmounting the last instance of the file system. Given that
// restriction, we do not need to lock access to the structure data
// when freeing it.
void
procfs_structure_free() {
    if (root_node != NULL) {
        // Release the root node. This recursively releases
        // all descendent nodes.
        release_node(root_node);
        root_node = NULL;
    }
}

// Gets the vnode type that is appropriate for a given structure node type.
enum vtype
vnode_type_for_structure_node_type(procfs_structure_node_type_t snode_type) {
    switch (snode_type) {
    case PROCFS_ROOT:       // FALLTHRU
    case PROCFS_PROCDIR:    // FALLTHRU
    case PROCFS_THREADDIR:  // FALLTHRU
    case PROCFS_DIR:        // FALLTHRU
    case PROCFS_DIR_THIS:   // FALLTHRU
    case PROCFS_DIR_PARENT: // FALLTHRU
    case PROCFS_FD_DIR:     // FALLTHRU
        return VDIR;
        
    case PROCFS_FILE:
        return VREG;
            
    case PROCFS_PROCNAME_DIR:   // FALLTHRU
    case PROCFS_CURPROC:
        return VLNK;
    }
   
    // Unknown type: make it a file.
    return VREG;
}

#pragma mark -
#pragma mark Creation of Structure Nodes

/*
 * Adds a node to the file system structure. This is a low-level
 * function that is called by add_file() and add_directory(). It
 * should not be called directly.
 */
STATIC procfs_structure_node_t *
add_node(procfs_structure_node_t *parent,
         const char *name,
         procfs_structure_node_type_t type,
         procfs_base_node_id_t node_id,
         uint16_t flags,
         size_t size,
         procfs_node_size_fn node_size_fn,
         procfs_read_data_fn node_read_data_fn) {
    procfs_structure_node_t *node = (procfs_structure_node_t *)OSMalloc(sizeof(procfs_structure_node_t), procfs_osmalloc_tag);
    if (node == NULL) {
        panic("Unable to allocate memory for procfs_structure_node_t");
    }
    
    bzero(node, sizeof(procfs_structure_node_t));
    node->psn_node_type = type;
    strlcpy(node->psn_name, name, sizeof(node->psn_name));
    node->psn_base_node_id = node_id;
    node->psn_flags = flags;
    node->psn_parent = parent;
    node->psn_node_size = size;
    node->psn_getsize_fn = node_size_fn;
    node->psn_read_data_fn = node_read_data_fn;
    
    TAILQ_INIT(&node->psn_children);
    if (parent != NULL) {
        // Add this node to the tail of its parent's child list.
        TAILQ_INSERT_TAIL(&parent->psn_children, node, psn_next);
        
        // Propagate the PSN_FLAG_PROCESS and PSN_FLAG_THREAD flags downward.
        node->psn_flags |= (parent->psn_flags & (PSN_FLAG_PROCESS | PSN_FLAG_THREAD));
    }
    return node;
}

/*
 * Adds a directory node to the file system structure. Since all directories
 * must have "." and ".." entries, these are added here by a recursive call
 * with the raw argument set to 1 to avoid infinite recursion.
 */
STATIC procfs_structure_node_t *
add_directory(procfs_structure_node_t *parent,
              const char *name,
              procfs_structure_node_type_t type,
              procfs_base_node_id_t node_id,
              uint16_t flags,
              boolean_t raw,
              procfs_node_size_fn node_size_fn,
              procfs_read_data_fn node_read_data_fn) {
    // Add the directory node.
    procfs_structure_node_t *snode = add_node(parent, name, type, node_id, flags, 0, node_size_fn, node_read_data_fn);
    
    // Add the "." and ".." directory entries, preserving the flags that indicate whether
    // the node is process- and/or thread-specific. The "raw" argument is used to stop
    // this being a recursive process.
    if (!raw) {
        add_directory(snode, ".", PROCFS_DIR_THIS, next_node_id++, flags & (PSN_FLAG_PROCESS | PSN_FLAG_THREAD), 1, NULL, NULL);
        add_directory(snode, "..", PROCFS_DIR_PARENT, next_node_id++, flags & (PSN_FLAG_PROCESS | PSN_FLAG_THREAD), 1, NULL, NULL);
    }
    return snode;
}

/*
 * Adds a file to the file system structure. Files are always leaf
 * elements (although that is not checked).
 */
STATIC procfs_structure_node_t *
add_file(procfs_structure_node_t *parent,
         const char *name,
         procfs_base_node_id_t node_id,
         uint16_t flags,
         size_t size,
         procfs_node_size_fn node_size_fn,
         procfs_read_data_fn node_read_data_fn) {
    return add_node(parent, name, PROCFS_FILE, node_id, flags, size, node_size_fn, node_read_data_fn);
}

#pragma mark -
#pragma mark Clean up of Structure Nodes

/*
 * Removes a node from the file system structure and releases its
 * memory. This happens only when the last instance of the file
 * system is unmounted.
 */
STATIC void
release_node(procfs_structure_node_t *snode) {
    // Remove from its parent's children list, if it has one.
    if (snode->psn_parent != NULL) {
        TAILQ_REMOVE(&snode->psn_parent->psn_children, snode, psn_next);
    }
    
    // Release all child nodes.
    procfs_structure_node_t *child = TAILQ_FIRST(&snode->psn_children);
    while (child != NULL) {
        TAILQ_REMOVE(&snode->psn_children, child, psn_next);
        release_node(child);
        child = TAILQ_FIRST(&snode->psn_children);
    }
    
    // Free this node's memory.
    OSFree(snode, sizeof(procfs_structure_node_t), procfs_osmalloc_tag);
}
