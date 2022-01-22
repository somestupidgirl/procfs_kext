#ifndef procfs_locks_h
#define procfs_locks_h

#include <kern/locks.h>
#include <sys/proc.h>

#include "procfs_thread.h"

void procfs_fdlock(proc_t p);
void procfs_fdunlock(proc_t p);
void procfs_fdlock_spin(proc_t p);
void procfs_list_lock(void);
void procfs_list_unlock(void);


#define thread_lock(thread) lck_mtx_lock(&(thread)->mutex)
#define thread_unlock(thread) lck_mtx_unlock(&(thread)->mutex)

#define task_lock(task) lck_mtx_lock(&(task)->lock)
#define task_unlock(task) lck_mtx_unlock(&(task)->lock)

#endif /* procfs_locks_h */
