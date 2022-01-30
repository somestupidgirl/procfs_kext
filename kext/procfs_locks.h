#ifndef procfs_locks_h
#define procfs_locks_h

void procfs_list_lock(void);
void procfs_list_unlock(void);
void procfs_fdlock(proc_t p);
void procfs_fdlock_spin(proc_t p);
void procfs_fdunlock(proc_t p);

#endif /* procfs_locks_h */
