#ifndef procfs_proc_h
#define procfs_proc_h

#include <sys/queue.h>

#define PROC_RETURNED                   (0)
#define PROC_RETURNED_DONE              (1)
#define PROC_CLAIMED                    (2)
#define PROC_CLAIMED_DONE               (3)

#define PID_MAX                          99999
#define NO_PID                          100000

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

#define SESS_LEADER(p, sessp)   ((sessp)->s_leader == (p))

LIST_HEAD(pidhashhead, proc) * pidhashtbl;
u_long pidhash;

#define PIDHASH(pid)    (&pidhashtbl[(pid) & pidhash])

/* list flags */
#define P_LIST_WAITING                  0x00000010
#define P_LIST_EXITED                   0x00000040
#define P_LIST_INCREATE                 0x00002000
#define P_LIST_PGRPTRANS                0x00040000
#define P_LIST_PGRPTRWAIT               0x00080000

/* local flags */
#define P_LTRANSCOMMIT                  0x00000020      /* process is committed to trans */
#define P_LINTRANSIT                    0x00000040      /* process in exec or in creation */
#define P_LTRANSWAIT                    0x00000080      /* waiting for trans to complete */
#define P_LTRACED                       0x00000400
#define P_LPPWAIT                       0x00002000

/* These flags are kept in extern_proc.p_flag. */
#define P_CONTROLT                      0x00000002      /* Has a controlling terminal */
#define P_SUGID                         0x00000100      /* Has set privileges since last exec */
#define P_SYSTEM                        0x00000200      /* Sys proc: no sigs, stats or swap */
#define P_EXEC                          0x00004000      /* Process called exec. */
#define P_THCWD                         0x01000000      /* process has thread cwd  */

/* Process control state for resource starvation */
#define P_PCTHROTTLE                    1
#define P_PCSUSP                        2
#define P_PCKILL                        3
#define P_PCMAX                         3

/* Process control action state on resrouce starvation */
#define PROC_CONTROL_STATE(p) (p->p_pcaction & P_PCMAX)
#define PROC_ACTION_STATE(p) ((p->p_pcaction >> 16) & P_PCMAX)

#endif /* procfs_proc_h */