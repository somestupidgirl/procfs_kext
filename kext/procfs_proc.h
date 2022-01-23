#ifndef procfs_iterate_h
#define procfs_iterate_h

typedef int (*proc_iterate_fn_t)(proc_t, void *);

#define PID_MAX         99999
#define NO_PID          100000

#pragma mark -
#pragma mark Function Declarations

proc_t proc_find_zombref(int pid);
void proc_drop_zombref(proc_t p);
void proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
void session_rele(struct session *sess);
void pg_rele(struct pgrp * pgrp);

#endif /* procfs_iterate_h */