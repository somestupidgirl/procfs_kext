/*
 * Copyright (c) 2015 Kim Topley
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * procfs_subr.c
 *
 * Utility functions for the ProcFS file system.
 */
#include <kern/thread.h>
#include <libkern/OSMalloc.h>
#include <libkext/libkext.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/task.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <miscfs/procfs/procfs.h>

#include "lib/symbols.h"

/* 
 * Allocate a pfsnode/vnode pair. Gets the vnode type that is appropriate
 * for a given structure node type.
 */
enum vtype
procfs_allocvp(pfstype pfs_type)
{
    switch (pfs_type) {
    case PFSroot:           /* FALLTHROUGH */
    case PFSproc:           /* FALLTHROUGH */
    case PFSthread:         /* FALLTHROUGH */
    case PFSdir:            /* FALLTHROUGH */
    case PFSdirthis:        /* FALLTHROUGH */
    case PFSdirparent:      /* FALLTHROUGH */
    case PFSfd:             /* FALLTHROUGH */
        return VDIR;

    case PFSfile:           /* FALLTHROUGH */
    case PFScpuinfo:        /* FALLTHROUGH */
        return VREG;

    case PFSprocnamedir:    /* FALLTHROUGH */
    case PFScurproc:        /* FALLTHROUGH */
        return VLNK;
    }

    // Unknown type: make it a file.
    return VREG;
}

/*
 * Given a vnode that corresponds to a pfsnode_t, returns the corresponding
 * process id and proc_t reference. If the node does not have a corresponding
 * process (i.e. it is the file system root node), the returned pid is 
 * PRNODE_NO_PID and the proc_t reference is NULL. If the process no longer
 * exists, returns ENOENT.
 */
int
procfs_get_process_info(vnode_t vp, pid_t *pidp, proc_t *procp)
{
    pfsnode_t *pfsnode = VTOPFS(vp);
    pfssnode_t *snode = pfsnode->node_structure_node;
    pfstype node_type = snode->psn_node_type;

    int pid = procfsnode_to_pid(pfsnode);
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
procfs_node_type_has_pid(pfstype node_type)
{
    return node_type != PFSroot && node_type != PFScurproc
        && node_type != PFSdir && node_type != PFScpuinfo
        && node_type != PFSversion;
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
procfs_get_node_fileid(pfsnode_t *pnp)
{
    pfsid_t node_id = pnp->node_id;
    return procfs_get_fileid(node_id.nodeid_pid, node_id.nodeid_objectid, pnp->node_structure_node->psn_base_node_id);
}

/*
 * Constructs a file id for a given process id, object id and structure node
 * base id. This may not be unique because we can only include part of the object
 * id. It should, however, be good enough.
 */
uint64_t
procfs_get_fileid(int pid, uint64_t objectid, pfsbaseid_t base_id)
{
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
procfs_atoi(const char *p, const char **end_ptr)
{
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
 * Function used to iterate the process list to collect
 * process ids. If the procfs_pidlist_data structure has
 * credentials, the process id id added only if it should
 * be accessible to an entity with those credentials.
 */
STATIC int
procfs_get_pid(proc_t p, void *udata)
{
    struct procfs_pidlist_data *data = udata;
    kauth_cred_t creds = data->creds;

    if (creds == NULL || procfs_check_can_access_process(creds, p) == 0) {
        if (data->num_procs >= data->max_procs) {
            // Workaround race-condition between read of nprocs and the kernel
            // spawning more processes.
            return PROC_RETURNED;
        }

        data->pids[data->num_procs] = proc_pid(p);
        data->num_procs++;
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
procfs_get_pids(pid_t **pidpp, int *pid_count, uint32_t *sizep, kauth_cred_t creds)
{
    uint32_t size = nprocs * sizeof(pid_t);
    pid_t *pidp = OSMalloc(size, procfs_osmalloc_tag);

    struct procfs_pidlist_data data;
    data.num_procs = 0;
    data.max_procs = nprocs;
    data.creds = creds;
    data.pids = pidp;

    proc_iterate(PROC_ALLPROCLIST, procfs_get_pid, &data, NULL, NULL);

    *pidpp = pidp;
    *sizep = size;
    *pid_count = data.num_procs;
}

/*
 * Frees a list of process id obtained from an earlier
 * invocation of procfs_get_pids().
 */
void
procfs_release_pids(pid_t *pidp, uint32_t size)
{
    OSFree(pidp, size, procfs_osmalloc_tag);
}

int
procfs_issuser(kauth_cred_t creds)
{
    if (kauth_cred_getuid(creds) == 0) {
        return 0;
    }
    return EPERM;
}

/*
 * Gets the number of active processes that are visible to a
 * process with given credentials.
 */
int
procfs_get_process_count(kauth_cred_t creds)
{
    pid_t *pidp;
    int process_count;
    uint32_t size;

    boolean_t is_suser = procfs_issuser(creds) == 0;

    procfs_get_pids(&pidp, &process_count, &size, is_suser ? NULL : creds);
    procfs_release_pids(pidp, size);
    if (pidp != NULL) {
        pidp = NULL;
    }
    
    return process_count;
}

/*
 * Gets a list of the thread ids for the threads belonging
 * to a given Mach task. The memory in which the thread list
 * is return is allocated by this function and must be freed
 * by calling procfs_release_thread_ids().
 */
int
procfs_get_thread_ids_for_task(task_t task, uint64_t **thread_ids, int *thread_count)
{
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
procfs_release_thread_ids(uint64_t *thread_ids, int thread_count)
{
    OSFree(thread_ids, thread_count * sizeof(uint64_t), procfs_osmalloc_tag);
}

/*
 * Get the number of threads for a given task.
 */
int
procfs_get_task_thread_count(task_t task)
{
    int thread_count = 0;
    uint64_t *thread_ids;

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
procfs_check_can_access_process(kauth_cred_t creds, proc_t p)
{
    posix_cred_t posix_creds = &creds->cr_posix;
    kauth_cred_t proc_cred = kauth_cred_proc_ref(p);

    // Allow access if the effective user id matches the
    // effective or real user id of the process.
    uid_t cred_euid = posix_creds->cr_uid;
    if (cred_euid == kauth_cred_getuid(proc_cred) || cred_euid == kauth_cred_getruid(proc_cred)) {
        return 0;
    }

    // Also allow access if the effective group id matches
    // the effective or saved group id of the process.
    gid_t cred_egid = posix_creds->cr_groups[0];
    if (cred_egid == kauth_cred_getgid(proc_cred) || cred_egid == kauth_cred_getrgid(proc_cred)) {
        return 0;
    }

    kauth_cred_unref(&proc_cred);

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
procfs_check_can_access_proc_pid(kauth_cred_t creds, pid_t pid)
{
    int error = 0;

    proc_t p = proc_find(pid);
    if (p != PROC_NULL) {
        error = procfs_check_can_access_process(creds, p);
        proc_rele(p);
    } else {
        error = ESRCH;
    }

    return error;
}
