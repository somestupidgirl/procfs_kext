#ifndef _procfs_info_h
#define _procfs_info_h

#include <sys/file_internal.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>

extern int procfs_proc_pidshortbsdinfo(proc_t p, struct proc_bsdshortinfo * pbsd_shortp, int zombie);
extern int procfs_proc_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo);
extern int procfs_proc_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo);
extern int procfs_fill_vnodeinfo(vnode_t vp, struct vnode_info *vi, vfs_context_t ctx);
extern void procfs_fill_fileinfo(struct fileproc * fp, proc_t proc, int fd, struct proc_fileinfo * fproc);

#define P_CONTROLT      0x00000002      /* Has a controlling terminal */
#define P_SUGID         0x00000100      /* Has set privileges since last exec */
#define P_SYSTEM        0x00000200      /* System process: no sigs, stats or swap */
#define P_LTRACED       0x00000400      /* process currently being traced, possibly by gdb */
#define P_LPPWAIT       0x00002000      /* */
#define P_EXEC          0x00004000      /* Process called exec. */
#define P_DELAYIDLESLEEP 0x00040000     /* Process is marked to delay idle sleep on disk IO */
#define P_THCWD         0x01000000      /* process has thread cwd  */

/* Process control state for resource starvation */
#define P_PCTHROTTLE    1
#define P_PCSUSP        2
#define P_PCKILL        3
#define P_PCMAX         3

/* Process control action state on resrouce starvation */
#define PROC_ACTION_MASK 0xffff0000;
#define PROC_CONTROL_STATE(p) (p->p_pcaction & P_PCMAX)
#define PROC_ACTION_STATE(p) ((p->p_pcaction >> 16) & P_PCMAX)

/* Status values. */
#define SIDL    1               /* Process being created by fork. */
#define SRUN    2               /* Currently runnable. */
#define SSLEEP  3               /* Sleeping on an address. */
#define SSTOP   4               /* Process debugging or suspension. */
#define SZOMB   5               /* Awaiting collection by parent. */

#ifndef _GUARDID_T
#define _GUARDID_T
typedef __uint64_t guardid_t;
#endif /* _GUARDID_T */

/*
 * Most fd's have an underlying fileproc struct; but some may be
 * guarded_fileproc structs which implement guarded fds.  The latter
 * struct (below) embeds the former.
 *
 * The two types should be distinguished by the "type" portion of fp_flags.
 * There's also a magic number to help catch misuse and bugs.
 *
 * This is a bit unpleasant, but results from the desire to allow
 * alternate file behaviours for a few file descriptors without
 * growing the fileproc data structure.
 */

struct guarded_fileproc {
    struct fileproc gf_fileproc;
    u_int           gf_attrs;
    guardid_t       gf_guard;
};

/*
 * Forbid close(2), and the implicit close() that a dup2(2) may do.
 * Forces close-on-fork to be set immutably too.
 */
#define GUARD_CLOSE         (1u << 0)

/*
 * Forbid dup(2), dup2(2), and fcntl(2) subcodes F_DUPFD, F_DUPFD_CLOEXEC
 * on a guarded fd. Also forbids open's of a guarded fd via /dev/fd/
 * (an implicit dup.)
 */
#define GUARD_DUP           (1u << 1)

/*
 * Forbid sending a guarded fd via a socket
 */
#define GUARD_SOCKET_IPC    (1u << 2)

/*
 * Forbid creating a fileport from a guarded fd
 */
#define GUARD_FILEPORT      (1u << 3)

#endif /* _procfs_info_h */