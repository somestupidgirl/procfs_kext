#ifndef _proc_locks_h
#define _proc_locks_h

extern void _proc_lock(proc_t p);
extern void _proc_unlock(proc_t p);
extern void _proc_spinlock(proc_t p);
extern void _proc_spinunlock(proc_t p);
extern void _proc_list_lock(void);
extern void _proc_list_unlock(void);
extern void _proc_klist_lock(void);
extern void _proc_klist_unlock(void);
extern void _proc_ucred_lock(proc_t p);
extern void _proc_ucred_unlock(proc_t p);
extern void _proc_fdlock(proc_t p);
extern void _proc_fdlock_spin(proc_t p);
extern void _proc_fdlock_assert(proc_t p, int assertflags);
extern void _proc_fdunlock(proc_t p);
extern void _pgrp_lock(struct pgrp * pgrp);
extern void _pgrp_unlock(struct pgrp * pgrp);
extern void _session_lock(struct session * sess);
extern void _session_unlock(struct session * sess);
extern void _tty_lock(struct tty *tp);
extern void _tty_unlock(struct tty *tp);

#endif /* _proc_locks_h */