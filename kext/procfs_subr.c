//
//  procfs_subr.c
//  ProcFS
//
//  Created by Kim Topley on 12/29/15.
//
//
/*
 * Utility functions for procfs.
 */
#include <libkern/OSMalloc.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <sys/ucred.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include "procfsnode.h"
#include "procfs_subr.h"

#pragma mark -
#pragma mark External References.

extern thread_t convert_port_to_thread(ipc_port_t port);

#pragma mark -
#pragma mark Function Prototypes.

struct procfs_pidlist_data;
STATIC int procfs_get_pid(proc_t p, struct procfs_pidlist_data *data);

/*
 * Given a vnode that corresponds to a procfsnode_t, returns the corresponding
 * process id and proc_t reference. If the node does not have a corresponding
 * process (i.e. it is the file system root node), the returned pid is 
 * PRNODE_NO_PID and the proc_t reference is NULL. If the process no longer
 * exists, returns ENOENT.
 */
int
procfs_get_process_info(vnode_t vp, pid_t *pidp, proc_t *procp) {
    procfsnode_t *procfs_node = vnode_to_procfsnode(vp);
    procfs_structure_node_t *snode = procfs_node->node_structure_node;
    procfs_structure_node_type_t node_type = snode->psn_node_type;
    
    pid_t pid = procfsnode_to_pid(procfs_node);
    proc_t p = pid == PRNODE_NO_PID ? NULL : proc_find(pid); // Process for the vnode, if there is one.
    if (p == NULL && procfs_node_type_has_pid(node_type)) {
        // Process must have gone -- return an error
        return ENOENT;
    }
    
    *procp = p;
    *pidp = pid;
    return 0;
}

/*
 * Returns whether a node of a given type must have an
 * associated process id.
 */
boolean_t
procfs_node_type_has_pid(procfs_structure_node_type_t node_type) {
    return node_type != PROCFS_ROOT && node_type != PROCFS_CURPROC
                    && node_type != PROCFS_DIR;
}

/*
 * Gets the file id for a given node. There is no obvious way to create
 * a unique and reproducible file id for a node that doesn't have any
 * persistent storage, so we synthesize one based on the base node id
 * from the file system structure, the owning process id if there is one
 * and the owning object id (which is a thread or a file descriptor). 
 * This may not be unique because we can only include part of the object id. 
 * It should, however, be good enough.
 */
uint64_t
procfs_get_node_fileid(procfsnode_t *pnp) {
    procfsnode_id_t node_id = pnp->node_id;
    return procfs_get_fileid(node_id.nodeid_pid, node_id.nodeid_objectid, pnp->node_structure_node->psn_base_node_id);
}

/*
 * Constructs a file id for a given process id, object id and structure node
 * base id. This may not be unique because we can only include part of the object
 * id. It should, however, be good enough.
 */
uint64_t
procfs_get_fileid(pid_t pid, uint64_t objectid, procfs_base_node_id_t base_id) {
    uint64_t id = base_id;
    if (pid != PRNODE_NO_PID) {
        id |= pid << 8;
    }
    id |= objectid << 24;
    return id;
}

/*
 * Attempts to convert a string to a positive integer. Returns
 * the value, or -1 if the string does not start with an integer
 * value. *end_ptr is set to point to the first non-numeric 
 * character encountered.
 */
int
procfs_atoi(const char *p, const char **end_ptr) {
    int value = 0;
    const char *next = p;
    char c;

    while ((c = *next++) != (char)0 && c >= '0' && c <= '9') {
        value = value * 10 + c - '0';
    }
    *end_ptr = next - 1;
    
    // Invalid if the first character was not a digit.
    return next == p + 1 ? -1 : value;
}

/*
 * Structure used to keep track of pid collection.
 */
struct procfs_pidlist_data {
    kauth_cred_t creds;     // Credential to use for access check, or NULL
    pid_t       *next_pid;  // Where to put the next process id.
};

/*
 * Function used to iterate the process list to collect
 * process ids. If the procfs_pidlist_data structure has
 * credentials, the process id id added only if it should
 * be accessible to an entity with those credentials.
 */
STATIC int
procfs_get_pid(proc_t p, struct procfs_pidlist_data *data) {
    kauth_cred_t creds = data->creds;
    if (creds == NULL || procfs_check_can_access_process(creds, p) == 0) {
        *data->next_pid++ = p->p_pid;
    }
    return PROC_RETURNED;
}

/*
 * Gets a list of all of the running processes in the system that
 * can be seen by a process with given credentials. If the creds
 * argument is NULL, no access check is made and the process ids
 * of all active processes are returned.
 * This function allocates memory for the list of pids and
 * returns it in the location pointed to by pidpp and the
 * number of valid entries in *pid_count. The total size of the
 * allocated memory is returned in *sizep. The caller must
 * call procfs_release_pids() to free the memory, passing in
 * the values that it received from this function. 
 */
void
procfs_get_pids(pid_t **pidpp, int *pid_count, uint32_t *sizep, kauth_cred_t creds) {
    uint32_t size = nprocs * sizeof(pid_t);
    pid_t *pidp = (pid_t *)OSMalloc(size, procfs_osmalloc_tag);
    
    struct procfs_pidlist_data data;
    data.creds = creds;
    data.next_pid = pidp;
    
    proc_iterate(PROC_ALLPROCLIST, (int (*)(proc_t, void *))&procfs_get_pid, &data, NULL, NULL);
    *pidpp = pidp;
    *sizep = size;
    *pid_count = (int)(data.next_pid - pidp);
}

/*
 * Frees a list of process id obtained from an earlier
 * invocation of procfs_get_pids().
 */
void
procfs_release_pids(pid_t *pidp, uint32_t size) {
    OSFree(pidp, size, procfs_osmalloc_tag);
}

/*
 * Gets the number of active processes that are visible to a
 * process with given credentials.
 */
int
procfs_get_process_count(kauth_cred_t creds) {
    pid_t *pidp;
    int process_count;
    uint32_t size;
    
    boolean_t is_suser = suser(creds, NULL) == 0;
    procfs_get_pids(&pidp, &process_count, &size, is_suser ? NULL : creds);
    procfs_release_pids(pidp, size);
    
    return process_count;
}

/*
 * Gets a list of the thread ids for the threads belonging
 * to a given Mach task. The memory in which the thread list
 * is return is allocated by this function and must be freed
 * by calling procfs_release_thread_ids().
 */
int
procfs_get_thread_ids_for_task(task_t task, uint64_t **thread_ids, int *thread_count) {
    int result = KERN_SUCCESS;
    thread_act_array_t threads;
    mach_msg_type_number_t count;
    
    // Get all of the threads in the task.
    if (task_threads(task, &threads, &count) == KERN_SUCCESS && count > 0) {
        uint64_t thread_id_info[THREAD_IDENTIFIER_INFO_COUNT];
        uint64_t *threadid_ptr = (uint64_t *)OSMalloc(count * sizeof(uint64_t), procfs_osmalloc_tag);
        *thread_ids = threadid_ptr;
        
        // For each thread, get identifier info and extract the thread id.
        for (unsigned int i = 0; i < count && result == KERN_SUCCESS; i++) {
            unsigned int thread_info_count = THREAD_IDENTIFIER_INFO_COUNT;
            ipc_port_t thread_port = (ipc_port_t)threads[i];
            thread_t thread = convert_port_to_thread(thread_port);
            if (thread != NULL) {
                result = thread_info(thread, THREAD_IDENTIFIER_INFO, (thread_info_t)&thread_id_info, &thread_info_count);
                if (result == KERN_SUCCESS) {
                    struct thread_identifier_info *idinfo = (struct thread_identifier_info *)thread_id_info;
                    *threadid_ptr++ = idinfo->thread_id;
                }
                thread_deallocate(thread);
            }
        }
        
        if (result == KERN_SUCCESS) {
            // We may have copied fewer threads than we expected, because some
            // may have terminated while we were looping over them. If so,
            // allocate a smaller memory region and copy everything over to it.
            unsigned int actual_count = (int)(threadid_ptr - *thread_ids);
            if (actual_count < count) {
                if (actual_count > 0) {
                    int size = actual_count * sizeof(uint64_t);
                    threadid_ptr = (uint64_t *)OSMalloc(size, procfs_osmalloc_tag);
                    bcopy(*thread_ids, threadid_ptr, size);
                }
                OSFree(*thread_ids, count * sizeof(uint64_t), procfs_osmalloc_tag);
                count = actual_count;
                *thread_ids = count > 0 ? threadid_ptr : NULL;
            }
        }
        
        // On failure, release the memory we allocated.
        if (result != KERN_SUCCESS) {
            procfs_release_thread_ids(*thread_ids, count);
            *thread_ids = NULL;
        }
    }
    
    *thread_count = result == KERN_SUCCESS ? count : 0;
    
    return result;
}

/*
 * Releases the memory allocated by an earlier successful call to
 * the get_thread_ids_for_task() function.
 */
void
procfs_release_thread_ids(uint64_t *thread_ids, int thread_count) {
    OSFree(thread_ids, thread_count * sizeof(uint64_t), procfs_osmalloc_tag);
}

/*
 * Get the number of threads for a given task.
 */
int
procfs_get_task_thread_count(task_t task) {
    uint64_t *thread_ids;
    int thread_count = 0;
    if (procfs_get_thread_ids_for_task(task, &thread_ids, &thread_count) == 0) {
        procfs_release_thread_ids(thread_ids, thread_count);
    }
    return thread_count;
}

/*
 * Determines whether an entity with given credentials can
 * access a given process. The determination is based on the 
 * real and effective user/group ids of the process. Returns 
 * 0 if access is allowed and EACCES otherwise.
 */
int
procfs_check_can_access_process(kauth_cred_t creds, proc_t p) {
    posix_cred_t posix_creds = &creds->cr_posix;
    
    // Allow access if the effective user id matches the
    // effective or real user id of the process.
    uid_t cred_euid = posix_creds->cr_uid;
    if (cred_euid == p->p_uid || cred_euid == p->p_ruid) {
        return 0;
    }
    
    // Also allow access if the effective group id matches
    // the effective or saved group id of the process.
    gid_t cred_egid = posix_creds->cr_groups[0];
    if (cred_egid == p->p_gid || cred_egid == p->p_rgid) {
        return 0;
    }
    return EACCES;
}


/*
 * Determines whether an entity with given credentials can
 * access the process with a given process id. The determination
 * is based on the real and effective user/group ids of the 
 * process. Returns 0 if access is allowed, ESRCH if there is
 * no process with the given pid and EACCES otherwise.
 */
int
procfs_check_can_access_proc_pid(kauth_cred_t creds, pid_t pid) {
    int error = ESRCH;
    proc_t p = proc_find(pid);
    if (p != NULL) {
        error = procfs_check_can_access_process(creds, p);
        proc_rele(p);
    }
    return error;
}



