#ifndef procfs_proc_h
#define procfs_proc_h

#include <sys/queue.h>

// malloc flags
#define M_PGRP 17
#define M_PROC 41
#define M_TTYS 65

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

#define SESS_LEADER(p, sessp)   ((sessp)->s_leader == (p))

#define PIDS_PER_ENTRY                  1021

#define PIDHASH(pid)    (&pidhashtbl[(pid) & pidhash])

LIST_HEAD(pidhashhead, proc) * pidhashtbl;
u_long pidhash;

#define PGRPID_DEAD                     0xdeaddead

/* p_listflag */
#define P_LIST_DRAIN                    0x00000001
#define P_LIST_DRAINWAIT                0x00000002
#define P_LIST_DRAINED                  0x00000004
#define P_LIST_DEAD                     0x00000008
#define P_LIST_WAITING                  0x00000010
#define P_LIST_EXITED                   0x00000040
#define P_LIST_CHILDDRSTART             0x00000080
#define P_LIST_CHILDDRAINED             0x00000100
#define P_LIST_CHILDDRWAIT              0x00000200
#define P_LIST_CHILDLKWAIT              0x00000400
#define P_LIST_DEADPARENT               0x00000800
#define P_LIST_PARENTREFWAIT            0x00001000
#define P_LIST_INCREATE                 0x00002000
/* 0x4000 &  0x8000 Not used */
#define P_LIST_INHASH                   0x00010000      /* process is in hash */
#define P_LIST_INPGRP                   0x00020000      /* process is in pgrp */
#define P_LIST_PGRPTRANS                0x00040000      /* pgrp is getting replaced */
#define P_LIST_PGRPTRWAIT               0x00080000      /* wait for pgrp replacement */
#define P_LIST_EXITCOUNT                0x00100000      /* counted for process exit */
#define P_LIST_REFWAIT                  0x00200000      /* wait to take a ref */

/* local flags */
#define P_LDELAYTERM                    0x00000001      /* */
#define P_LNOZOMB                       0x00000002      /* */
#define P_LTERM                         0x00000004      /* */
#define P_LEXIT                         0x00000008      /* */
#define P_LPEXIT                        0x00000010
#define P_LTRANSCOMMIT                  0x00000020      /* process is committed to trans */
#define P_LINTRANSIT                    0x00000040      /* process in exec or in creation */
#define P_LTRANSWAIT                    0x00000080      /* waiting for trans to complete */
#define P_LVFORK                        0x00000100      /* parent proc of a vfork */
#define P_LINVFORK                      0x00000200      /* child proc of a vfork */
#define P_LTRACED                       0x00000400      /* */
#define P_LSIGEXC                       0x00000800      /* */
#define P_LNOATTACH                     0x00001000      /* */
#define P_LPPWAIT                       0x00002000      /* */
#define P_LKQWDRAIN                     0x00004000
#define P_LKQWDRAINWAIT                 0x00008000
#define P_LKQWDEAD                      0x00010000
#define P_LLIMCHANGE                    0x00020000      /* process is changing its plimit (rlim_cur, rlim_max) */
#define P_LLIMWAIT                      0x00040000
#define P_LWAITED                       0x00080000
#define P_LINSIGNAL                     0x00100000
#define P_LCUSTOM_STACK                 0x00200000      /* process is using custom stack size */
#define P_LRAGE_VNODES                  0x00400000
#define P_LREGISTER                     0x00800000      /* thread start fns registered  */
#define P_LVMRSRCOWNER                  0x01000000      /* can handle the resource ownership of  */
#define P_LTERM_DECRYPTFAIL             0x04000000      /* process terminating due to key failure to decrypt */
#define P_LTERM_JETSAM                  0x08000000      /* process is being jetsam'd */

#define P_JETSAM_VMPAGESHORTAGE         0x00000000      /* jetsam: lowest jetsam priority proc, killed due to vm page shortage */
#define P_JETSAM_VMTHRASHING            0x10000000      /* jetsam: lowest jetsam priority proc, killed due to vm thrashing */
#define P_JETSAM_HIWAT                  0x20000000      /* jetsam: high water mark */
#define P_JETSAM_PID                    0x30000000      /* jetsam: pid */
#define P_JETSAM_IDLEEXIT               0x40000000      /* jetsam: idle exit */
#define P_JETSAM_VNODE                  0x50000000      /* jetsam: vnode kill */
#define P_JETSAM_FCTHRASHING            0x60000000      /* jetsam: lowest jetsam priority proc, killed due to filecache thrashing */
#define P_JETSAM_MASK                   0x70000000      /* jetsam type mask */
#define P_LNSPACE_RESOLVER              0x80000000      /* process is the namespace resolver */

/* Process control state for resource starvation */
#define P_PCTHROTTLE    1
#define P_PCSUSP        2
#define P_PCKILL        3
#define P_PCMAX         3

/* Process control action state on resrouce starvation */
#define PROC_ACTION_MASK 0xffff0000;
#define PROC_CONTROL_STATE(p) (p->p_pcaction & P_PCMAX)
#define PROC_ACTION_STATE(p) ((p->p_pcaction >> 16) & P_PCMAX)
#define PROC_SETACTION_STATE(p) (p->p_pcaction = (PROC_CONTROL_STATE(p) | (PROC_CONTROL_STATE(p) << 16)))
#define PROC_RESETACTION_STATE(p) (p->p_pcaction = PROC_CONTROL_STATE(p))

/* Process exit reason macros */
#define PROC_HAS_EXITREASON(p) (p->p_exit_reason != OS_REASON_NULL)
#define PROC_EXITREASON_FLAGS(p) p->p_exit_reason->osr_flags

/* additional process flags */
#define P_LADVLOCK              0x01
#define P_LXBKIDLEINPROG        0x02

/* p_vfs_iopolicy flags */
#define P_VFS_IOPOLICY_FORCE_HFS_CASE_SENSITIVITY       0x0001
#define P_VFS_IOPOLICY_ATIME_UPDATES                    0x0002
#define P_VFS_IOPOLICY_MATERIALIZE_DATALESS_FILES       0x0004
#define P_VFS_IOPOLICY_STATFS_NO_DATA_VOLUME            0x0008
#define P_VFS_IOPOLICY_TRIGGER_RESOLVE_DISABLE          0x0010
#define P_VFS_IOPOLICY_IGNORE_CONTENT_PROTECTION        0x0020
#define P_VFS_IOPOLICY_VALID_MASK                       (P_VFS_IOPOLICY_ATIME_UPDATES | P_VFS_IOPOLICY_FORCE_HFS_CASE_SENSITIVITY | P_VFS_IOPOLICY_MATERIALIZE_DATALESS_FILES | P_VFS_IOPOLICY_STATFS_NO_DATA_VOLUME | P_VFS_IOPOLICY_TRIGGER_RESOLVE_DISABLE | P_VFS_IOPOLICY_IGNORE_CONTENT_PROTECTION)

/* process creation arguments */
#define PROC_CREATE_FORK        0       /* independent child (running) */
#define PROC_CREATE_SPAWN       1       /* independent child (suspended) */
#define PROC_CREATE_VFORK       2       /* child borrows context */

/* Status values. */
#define SIDL    1               /* Process being created by fork. */
#define SRUN    2               /* Currently runnable. */
#define SSLEEP  3               /* Sleeping on an address. */
#define SSTOP   4               /* Process debugging or suspension. */
#define SZOMB   5               /* Awaiting collection by parent. */

/* These flags are kept in extern_proc.p_flag. */
#define P_ADVLOCK       0x00000001      /* Process may hold POSIX adv. lock */
#define P_CONTROLT      0x00000002      /* Has a controlling terminal */
#define P_LP64          0x00000004      /* Process is LP64 */
#define P_NOCLDSTOP     0x00000008      /* No SIGCHLD when children stop */

#define P_PPWAIT        0x00000010      /* Parent waiting for chld exec/exit */
#define P_PROFIL        0x00000020      /* Has started profiling */
#define P_SELECT        0x00000040      /* Selecting; wakeup/waiting danger */
#define P_CONTINUED     0x00000080      /* Process was stopped and continued */

#define P_SUGID         0x00000100      /* Has set privileges since last exec */
#define P_SYSTEM        0x00000200      /* Sys proc: no sigs, stats or swap */
#define P_TIMEOUT       0x00000400      /* Timing out during sleep */
#define P_TRACED        0x00000800      /* Debugged process being traced */

#define P_DISABLE_ASLR  0x00001000      /* Disable address space layout randomization */
#define P_WEXIT         0x00002000      /* Working on exiting */
#define P_EXEC          0x00004000      /* Process called exec. */

/* Should be moved to machine-dependent areas. */
#define P_OWEUPC        0x00008000      /* Owe process an addupc() call at next ast. */

#define P_AFFINITY      0x00010000      /* xxx */
#define P_TRANSLATED    0x00020000      /* xxx */
#define P_CLASSIC       P_TRANSLATED    /* xxx */

#define P_DELAYIDLESLEEP 0x00040000     /* Process is marked to delay idle sleep on disk IO */
#define P_CHECKOPENEVT  0x00080000      /* check if a vnode has the OPENEVT flag set on open */

#define P_DEPENDENCY_CAPABLE    0x00100000      /* process is ok to call vfs_markdependency() */
#define P_REBOOT        0x00200000      /* Process called reboot() */
#define P_RESV6         0x00400000      /* used to be P_TBE */
#define P_RESV7         0x00800000      /* (P_SIGEXC)signal exceptions */

#define P_THCWD         0x01000000      /* process has thread cwd  */
#define P_RESV9         0x02000000      /* (P_VFORK)process has vfork children */
#define P_ADOPTPERSONA  0x04000000      /* process adopted a persona (used to be P_NOATTACH) */
#define P_RESV11        0x08000000      /* (P_INVFORK) proc in vfork */

#define P_NOSHLIB       0x10000000      /* no shared libs are in use for proc */
                                        /* flag set on exec */
#define P_FORCEQUOTA    0x20000000      /* Force quota for root */
#define P_NOCLDWAIT     0x40000000      /* No zombies when chil procs exit */
#define P_NOREMOTEHANG  0x80000000      /* Don't hang on remote FS ops */

#define P_INMEM         0               /* Obsolete: retained for compilation */
#define P_NOSWAP        0               /* Obsolete: retained for compilation */
#define P_PHYSIO        0               /* Obsolete: retained for compilation */
#define P_FSTRACE       0               /* Obsolete: retained for compilation */
#define P_SSTEP         0               /* Obsolete: retained for compilation */

#define P_DIRTY_TRACK                           0x00000001      /* track dirty state */
#define P_DIRTY_ALLOW_IDLE_EXIT                 0x00000002      /* process can be idle-exited when clean */
#define P_DIRTY_DEFER                           0x00000004      /* defer initial opt-in to idle-exit */
#define P_DIRTY                                 0x00000008      /* process is dirty */
#define P_DIRTY_SHUTDOWN                        0x00000010      /* process is dirty during shutdown */
#define P_DIRTY_TERMINATED                      0x00000020      /* process has been marked for termination */
#define P_DIRTY_BUSY                            0x00000040      /* serialization flag */
#define P_DIRTY_MARKED                          0x00000080      /* marked dirty previously */
#define P_DIRTY_AGING_IN_PROGRESS               0x00000100      /* aging in one of the 'aging bands' */
#define P_DIRTY_LAUNCH_IN_PROGRESS              0x00000200      /* launch is in progress */
#define P_DIRTY_DEFER_ALWAYS                    0x00000400      /* defer going to idle-exit after every dirty->clean transition.
                                                                 * For legacy jetsam policy only. This is the default with the other policies.*/
#define P_DIRTY_IS_DIRTY                        (P_DIRTY | P_DIRTY_SHUTDOWN)
#define P_DIRTY_IDLE_EXIT_ENABLED               (P_DIRTY_TRACK|P_DIRTY_ALLOW_IDLE_EXIT)

#endif /* procfs_proc_h */