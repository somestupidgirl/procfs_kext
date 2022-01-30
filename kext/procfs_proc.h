#ifndef procfs_proc_h
#define procfs_proc_h

#include <sys/queue.h>

#define PROC_RETURNED                   (0)
#define PROC_RETURNED_DONE              (1)
#define PROC_CLAIMED                    (2)
#define PROC_CLAIMED_DONE               (3)

typedef int (*proc_iterate_fn_t)(proc_t, void *);

#define PROC_ALLPROCLIST                (1U << 0)
#define PROC_ZOMBPROCLIST               (1U << 1)
#define PROC_NOWAITTRANS                (1U << 2)

LIST_HEAD(proclist, proc);
struct proclist allproc;
struct proclist zombproc;

#define ALLPROC_FOREACH(var) \
    LIST_FOREACH((var), &allproc, p_list)

#define ZOMBPROC_FOREACH(var) \
    LIST_FOREACH((var), &zombproc, p_list)

#define PID_MAX                         99999
#define NO_PID                          100000

#define PIDS_PER_ENTRY                  1021

typedef struct pidlist_entry {
    SLIST_ENTRY(pidlist_entry) pe_link;
    u_int pe_nused;
    pid_t pe_pid[PIDS_PER_ENTRY];
} pidlist_entry_t;

typedef struct {
    SLIST_HEAD(, pidlist_entry) pl_head;
    struct pidlist_entry *pl_active;
    u_int pl_nalloc;
} pidlist_t;

#define PIDHASH(pid)    (&pidhashtbl[(pid) & pidhash])

LIST_HEAD(pidhashhead, proc) * pidhashtbl;
u_long pidhash;

#define P_LIST_WAITING                  0x00000010
#define P_LIST_EXITED                   0x00000040
#define P_LIST_INCREATE                 0x00002000

#define P_CONTROLT                      0x00000002
#define P_LP64                          0x00000004
#define P_LEXIT                         0x00000008
#define P_LTRANSCOMMIT                  0x00000020
#define P_LINTRANSIT                    0x00000040
#define P_LTRANSWAIT                    0x00000080
#define P_SUGID                         0x00000100
#define P_SYSTEM                        0x00000200
#define P_LTRACED                       0x00000400
#define P_LPPWAIT                       0x00002000
#define P_EXEC                          0x00004000
#define P_THCWD                         0x01000000

#endif /* procfs_proc_h */