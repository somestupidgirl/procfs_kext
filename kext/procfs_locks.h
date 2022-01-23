#ifndef procfs_locks_h
#define procfs_locks_h

#include <kern/locks.h>
#include <sys/proc.h>

#include "procfs_thread.h"

void proc_fdlock(proc_t p);
void proc_fdunlock(proc_t p);
void proc_fdlock_spin(proc_t p);

void proc_list_lock(void);
void proc_list_unlock(void);

void pgrp_lock(struct pgrp * pgrp);
void pgrp_unlock(struct pgrp * pgrp);

void session_lock(struct session * sess);
void session_unlock(struct session * sess);

#define thread_lock(thread) lck_mtx_lock(&(thread)->mutex)
#define thread_unlock(thread) lck_mtx_unlock(&(thread)->mutex)

#define task_lock(task) lck_mtx_lock(&(task)->lock)
#define task_unlock(task) lck_mtx_unlock(&(task)->lock)

#endif /* procfs_locks_h */
