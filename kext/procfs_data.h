//
//  procfs_data.h
//  ProcFS
//
//  Created by Kim Topley on 1/10/16.
//
//

#ifndef procfs_data_h
#define procfs_data_h

typedef struct procfsnode procfsnode_t;

// Functions that copy procfsnode_t data to a buffer described by a uio_t structure.
extern int procfs_read_pid_data(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_ppid_data(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_pgid_data(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_sid_data(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_tty_data(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_proc_info(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_task_info(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_thread_info(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_fd_data(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);
extern int procfs_read_socket_data(procfsnode_t *pnp, uio_t uio, vfs_context_t ctx);

// Functions that return the data size for a node.
extern size_t procfs_get_node_size_attr(procfsnode_t *pnp, kauth_cred_t creds);
extern size_t procfs_process_node_size(procfsnode_t *pnp, kauth_cred_t creds);
extern size_t procfs_thread_node_size(procfsnode_t *pnp, kauth_cred_t creds);
extern size_t procfs_fd_node_size(procfsnode_t *pnp, kauth_cred_t creds);

#endif /* procfs_data_h */
