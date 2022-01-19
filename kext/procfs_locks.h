#ifndef procfs_locks_h
#define procfs_locks_h

#include <sys/kernel_types.h>

void procfs_fdlock(proc_t p);
void procfs_fdunlock(proc_t p);
void procfs_fdlock_spin(proc_t p);
void procfs_list_lock(void);
void procfs_list_unlock(void);

#endif /* procfs_locks_h */
