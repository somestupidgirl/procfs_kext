#ifndef procfs_locks_h
#define procfs_locks_h

#include <sys/kernel_types.h>

void procfs_fdlock(proc_t p);
void procfs_fdunlock(proc_t p);
void procfs_fdlock_spin(proc_t p);
void procfs_list_lock(void);
void procfs_list_unlock(void);

struct hslock {
	uintptr_t       					 lock_data;
};

typedef struct hslock hw_lock_data_t, 	*hw_lock_t;

typedef struct slock {
	hw_lock_data_t                       interlock;
} usimple_lock_data_t,                  *usimple_lock_t;

extern void usimple_lock(usimple_lock_t, lck_grp_t*);
extern void usimple_unlock(usimple_lock_t);

#define simple_lock(l, grp)          	 usimple_lock(l, grp)
#define simple_unlock(l)        		 usimple_unlock(l)
#define thread_lock(th)                  simple_lock(&(th)->sched_lock, &thread_lck_grp)
#define thread_unlock(th)                simple_unlock(&(th)->sched_lock)

#endif /* procfs_locks_h */
