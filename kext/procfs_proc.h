#ifndef procfs_iterate_h
#define procfs_iterate_h

typedef int (*procfs_proc_iterate_fn_t)(proc_t, void *);

#pragma mark -
#pragma mark Function Declarations

proc_t procfs_proc_find_zombref(int pid);
void procfs_proc_drop_zombref(proc_t p);
void procfs_proc_iterate(unsigned int flags, procfs_proc_iterate_fn_t callout, void *arg, procfs_proc_iterate_fn_t filterfn, void *filterarg);
void procfs_session_rele(struct session *sess);

#endif /* procfs_iterate_h */