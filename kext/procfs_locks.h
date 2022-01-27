#ifndef procfs_locks_h
#define procfs_locks_h

void proc_list_lock(void);
void proc_list_unlock(void);
void proc_fdlock(proc_t p);
void proc_fdlock_spin(proc_t p);
void proc_fdunlock(proc_t p);

#endif /* procfs_locks_h */
