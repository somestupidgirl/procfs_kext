//
//  procfs_subr.h
//  ProcFS
//
//  Created by Kim Topley on 12/29/15.
//
//

#ifndef procfs_subr_h
#define procfs_subr_h

#include <sys/kernel_types.h>

extern boolean_t procfs_node_type_has_pid(procfs_structure_node_type_t node_type);
extern int procfs_get_process_info(vnode_t vp, pid_t *pidp, proc_t *procp);
extern uint64_t procfs_get_node_fileid(procfsnode_t *pnp);
extern uint64_t procfs_get_fileid(pid_t pid, uint64_t objectid, procfs_base_node_id_t base_id);
extern int procfs_atoi(const char *p, const char **end_ptr);
extern void procfs_get_pids(pid_t **pidpp, int *pid_count, uint32_t *sizep, kauth_cred_t creds);
extern void procfs_release_pids(pid_t *pidp, uint32_t size);
extern int procfs_get_thread_ids_for_task(task_t task, uint64_t **thread_ids, int *thread_count);
extern void procfs_release_thread_ids(uint64_t *thread_ids, int thread_count);
extern int procfs_check_can_access_process(kauth_cred_t creds, proc_t p);
extern int procfs_check_can_access_proc_pid(kauth_cred_t creds, pid_t pid);
extern int procfs_get_process_count(kauth_cred_t creds);
extern int procfs_get_task_thread_count(task_t task);

#endif /* procfs_subr_h */
