#ifndef _kern_proc_h
#define _kern_proc_h

extern int _proc_transstart(proc_t p, int locked, int non_blocking);
extern void _proc_transcommit(proc_t p, int locked);
extern void _proc_transend(proc_t p, int locked);
extern int _proc_transwait(proc_t p, int locked);
extern task_t _proc_task(proc_t proc);
extern proc_t _pfind_locked(pid_t pid);
extern proc_t _proc_find_zombref(int pid);
extern void _proc_drop_zombref(proc_t p);
extern struct pgrp *_proc_pgrp(proc_t p);
extern void _pg_rele(struct pgrp * pgrp);
extern struct session *_proc_session(proc_t p);
extern void _session_rele(struct session *sess);

// Iteration
extern void _proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);
extern void _pgrp_iterate(struct pgrp *pgrp, unsigned int flags, proc_iterate_fn_t callout, void * arg, proc_iterate_fn_t filterfn, void * filterarg);

#endif /* _kern_proc_h */