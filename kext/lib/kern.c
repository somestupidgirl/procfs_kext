#include <os/log.h>
#include <os/refcnt.h>
#include <sys/fcntl.h>
#include <sys/guarded.h>
#include <sys/file.h>
#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/mount_internal.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/vnode_internal.h>
#include <mach/mach_types.h>

#include "symbols.h"

/*
 * =========== From bsd/kern/bsd_init.c ===========
 */
proc_t XNU_PTRAUTH_SIGNED_PTR("initproc") initproc;

/*
 * =========== From bsd/conf/param.c ===========
 */
#if !defined(__x86_64__)
#define NPROC 1000          /* Account for DEFAULT_TOTAL_CORPSES_ALLOWED by making this slightly lower than we can. */
#define NPROC_PER_UID 950
#else
#define NPROC (20 + 32 * 32)
#define NPROC_PER_UID (NPROC/2)
#endif

/* NOTE: maxproc and hard_maxproc values are subject to device specific scaling in bsd_scale_setup */
#define HNPROC 2500     /* based on thread_max */
int     maxproc = NPROC;
int     maxprocperuid = NPROC_PER_UID;

#if !defined(__x86_64__)
int hard_maxproc = NPROC;       /* hardcoded limit -- for ARM the number of processes is limited by the ASID space */
#else
int hard_maxproc = HNPROC;      /* hardcoded limit */
#endif

int nprocs = 0; /* XXX */

/*
 * =========== From bsd/kern/kern_synch.c ===========
 */
extern void compute_averunnable(void *);        /* XXX */

struct loadavg averunnable =
{ {0, 0, 0}, FSCALE };                  /* load average, of runnable procs */

/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
static fixpt_t cexp[3] = {
    (fixpt_t)(0.9200444146293232 * FSCALE), /* exp(-1/12) */
    (fixpt_t)(0.9834714538216174 * FSCALE), /* exp(-1/60) */
    (fixpt_t)(0.9944598480048967 * FSCALE), /* exp(-1/180) */
};

void
compute_averunnable(void *arg)
{
    unsigned int            nrun = *(unsigned int *)arg;
    struct loadavg          *avg = &averunnable;
    int             i;

    for (i = 0; i < 3; i++) {
        avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
            nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;
    }
}

/*
 * =========== From bsd/kern/kern_proc.c ===========
 */
/*
 * The pidlist_* routines support the functions in this file that
 * walk lists of processes applying filters and callouts to the
 * elements of the list.
 *
 * A prior implementation used a single linear array, which can be
 * tricky to allocate on large systems. This implementation creates
 * an SLIST of modestly sized arrays of PIDS_PER_ENTRY elements.
 *
 * The array should be sized large enough to keep the overhead of
 * walking the list low, but small enough that blocking allocations of
 * pidlist_entry_t structures always succeed.
 */

#define PIDS_PER_ENTRY 1021

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

static __inline__ pidlist_t *
pidlist_init(pidlist_t *pl)
{
    SLIST_INIT(&pl->pl_head);
    pl->pl_active = NULL;
    pl->pl_nalloc = 0;
    return pl;
}

static u_int
pidlist_alloc(pidlist_t *pl, u_int needed)
{
    while (pl->pl_nalloc < needed) {
        pidlist_entry_t *pe = kalloc_type(pidlist_entry_t,
            Z_WAITOK | Z_ZERO | Z_NOFAIL);
        SLIST_INSERT_HEAD(&pl->pl_head, pe, pe_link);
        pl->pl_nalloc += (sizeof(pe->pe_pid) / sizeof(pe->pe_pid[0]));
    }
    return pl->pl_nalloc;
}

static void
pidlist_free(pidlist_t *pl)
{
    pidlist_entry_t *pe;
    while (NULL != (pe = SLIST_FIRST(&pl->pl_head))) {
        SLIST_FIRST(&pl->pl_head) = SLIST_NEXT(pe, pe_link);
        kfree_type(pidlist_entry_t, pe);
    }
    pl->pl_nalloc = 0;
}

static __inline__ void
pidlist_set_active(pidlist_t *pl)
{
    pl->pl_active = SLIST_FIRST(&pl->pl_head);
    assert(pl->pl_active);
}

static void
pidlist_add_pid(pidlist_t *pl, pid_t pid)
{
    pidlist_entry_t *pe = pl->pl_active;
    if (pe->pe_nused >= sizeof(pe->pe_pid) / sizeof(pe->pe_pid[0])) {
        if (NULL == (pe = SLIST_NEXT(pe, pe_link))) {
            panic("pidlist allocation exhausted");
        }
        pl->pl_active = pe;
    }
    pe->pe_pid[pe->pe_nused++] = pid;
}

static __inline__ u_int
pidlist_nalloc(const pidlist_t *pl)
{
    return pl->pl_nalloc;
}

struct proclist allproc = LIST_HEAD_INITIALIZER(allproc);
struct proclist zombproc = LIST_HEAD_INITIALIZER(zombproc);

void
proc_iterate(
    unsigned int flags,
    proc_iterate_fn_t callout,
    void *arg,
    proc_iterate_fn_t filterfn,
    void *filterarg)
{
    pidlist_t pid_list, *pl = pidlist_init(&pid_list);
    u_int pid_count_available = 0;

    assert(callout != NULL);

    /* allocate outside of the proc_list_lock */
    for (;;) {
        proc_list_lock();
        pid_count_available = nprocs + 1; /* kernel_task not counted in nprocs */
        assert(pid_count_available > 0);
        if (pidlist_nalloc(pl) >= pid_count_available) {
            break;
        }
        proc_list_unlock();

        pidlist_alloc(pl, pid_count_available);
    }
    pidlist_set_active(pl);

    /* filter pids into the pid_list */

    u_int pid_count = 0;
    if (flags & PROC_ALLPROCLIST) {
        proc_t p;
        ALLPROC_FOREACH(p) {
            /* ignore processes that are being forked */
            if (p->p_stat == SIDL || proc_is_shadow(p)) {
                continue;
            }
            if ((filterfn != NULL) && (filterfn(p, filterarg) == 0)) {
                continue;
            }
            pidlist_add_pid(pl, proc_pid(p));
            if (++pid_count >= pid_count_available) {
                break;
            }
        }
    }

    if ((pid_count < pid_count_available) &&
        (flags & PROC_ZOMBPROCLIST)) {
        proc_t p;
        ZOMBPROC_FOREACH(p) {
            if (proc_is_shadow(p)) {
                continue;
            }
            if ((filterfn != NULL) && (filterfn(p, filterarg) == 0)) {
                continue;
            }
            pidlist_add_pid(pl, proc_pid(p));
            if (++pid_count >= pid_count_available) {
                break;
            }
        }
    }

    proc_list_unlock();

    /* call callout on processes in the pid_list */

    const pidlist_entry_t *pe;
    SLIST_FOREACH(pe, &(pl->pl_head), pe_link) {
        for (u_int i = 0; i < pe->pe_nused; i++) {
            const pid_t pid = pe->pe_pid[i];
            proc_t p = proc_find(pid);
            if (p) {
                if ((flags & PROC_NOWAITTRANS) == 0) {
                    proc_transwait(p, 0);
                }
                const int callout_ret = callout(p, arg);

                switch (callout_ret) {
                case PROC_RETURNED_DONE:
                    proc_rele(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED_DONE:
                    goto out;

                case PROC_RETURNED:
                    proc_rele(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED:
                    break;
                default:
                    panic("%s: callout =%d for pid %d",
                        __func__, callout_ret, pid);
                    break;
                }
            } else if (flags & PROC_ZOMBPROCLIST) {
                p = proc_find_zombref(pid);
                if (!p) {
                    continue;
                }
                const int callout_ret = callout(p, arg);

                switch (callout_ret) {
                case PROC_RETURNED_DONE:
                    proc_drop_zombref(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED_DONE:
                    goto out;

                case PROC_RETURNED:
                    proc_drop_zombref(p);
                    OS_FALLTHROUGH;
                case PROC_CLAIMED:
                    break;
                default:
                    panic("%s: callout =%d for zombie %d",
                        __func__, callout_ret, pid);
                    break;
                }
            }
        }
    }
out:
    pidlist_free(pl);
}

/*
 * =========== From bsd/kern/proc_info.c ===========
 */
int
proc_pidshortbsdinfo(proc_t p, struct proc_bsdshortinfo * pbsd_shortp, int zombie)
{
    unsigned int proc_flag = p->p_flag;
    uint32_t proc_status = (uint32_t)p->p_stat;

    bzero(pbsd_shortp, sizeof(struct proc_bsdshortinfo));
    pbsd_shortp->pbsi_pid = (uint32_t)proc_pid(p);
    pbsd_shortp->pbsi_ppid = (uint32_t)proc_ppid(p);
    pbsd_shortp->pbsi_pgid = (uint32_t)proc_pgrpid(p);
    pbsd_shortp->pbsi_status = proc_status;

    bcopy(&p->p_comm, &pbsd_shortp->pbsi_comm[0], MAXCOMLEN);
    pbsd_shortp->pbsi_comm[MAXCOMLEN - 1] = '\0';

    kauth_cred_t my_cred = kauth_cred_proc_ref(p);
    pbsd_shortp->pbsi_uid = kauth_cred_getuid(my_cred);
    pbsd_shortp->pbsi_gid = kauth_cred_getgid(my_cred);
    pbsd_shortp->pbsi_ruid = kauth_cred_getruid(my_cred);
    pbsd_shortp->pbsi_rgid = kauth_cred_getrgid(my_cred);
    pbsd_shortp->pbsi_svuid = kauth_cred_getsvuid(my_cred);
    pbsd_shortp->pbsi_svgid = kauth_cred_getsvgid(my_cred);
    kauth_cred_unref(&my_cred);

    pbsd_shortp->pbsi_flags = 0;

    if ((proc_flag & P_SYSTEM) == P_SYSTEM) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_SYSTEM;
    }

    if ((proc_flag & P_LTRACED) == P_LTRACED) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_TRACED;
    }

    if (proc_exiting(p) != 0) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_INEXIT;
    }

    if ((proc_flag & P_LPPWAIT) == P_LPPWAIT) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PPWAIT;
    }

    if (proc_is64bit(p) != 0) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_LP64;
    }

    if ((proc_flag & P_CONTROLT) == P_CONTROLT) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_CONTROLT;
    }

    if ((proc_flag & P_THCWD) == P_THCWD) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_THCWD;
    }

    if (_proc_issetugid != NULL && proc_issetugid(p) != 0)  {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PSUGID;
    }

    if ((proc_flag & P_EXEC) == P_EXEC) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_EXEC;
    }

    if ((proc_flag & P_DELAYIDLESLEEP) == P_DELAYIDLESLEEP) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_DELAYIDLESLEEP;
    }

    switch (PROC_CONTROL_STATE(p)) {
    case P_PCTHROTTLE:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PC_THROTTLE;
        break;
    case P_PCSUSP:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PC_SUSP;
        break;
    case P_PCKILL:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PC_KILL;
        break;
    };

    switch (PROC_ACTION_STATE(p)) {
    case P_PCTHROTTLE:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PA_THROTTLE;
        break;
    case P_PCSUSP:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PA_SUSP;
        break;
    };

    /* if process is a zombie skip bg state */
    if (_proc_task != NULL && _proc_get_darwinbgstate != NULL &&
        (zombie == 0) && (proc_status != SZOMB) && (proc_task(p) != TASK_NULL)) {
        proc_get_darwinbgstate(proc_task(p), &pbsd_shortp->pbsi_flags);
    }

    return 0;
}

int
proc_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo)
{
    bzero(ptinfo, sizeof(struct proc_taskinfo));
    if (_proc_task != NULL && _fill_taskprocinfo != NULL) {
        fill_taskprocinfo(proc_task(p), (struct proc_taskinfo_internal *)ptinfo);
    }

    return 0;
}

int
proc_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo)
{
    bzero(pthinfo, sizeof(struct proc_threadinfo));
    if (_proc_task != NULL && _fill_taskthreadinfo != NULL) {
        fill_taskthreadinfo(proc_task(p), (uint64_t)arg, thuniqueid, (struct proc_threadinfo_internal *)pthinfo, NULL, NULL);
    }

    return 0;
}

/*
 * copy stat64 structure into vinfo_stat structure.
 */
static void
munge_vinfo_stat(struct stat64 *sbp, struct vinfo_stat *vsbp)
{
    bzero(vsbp, sizeof(struct vinfo_stat));

    vsbp->vst_dev = sbp->st_dev;
    vsbp->vst_mode = sbp->st_mode;
    vsbp->vst_nlink = sbp->st_nlink;
    vsbp->vst_ino = sbp->st_ino;
    vsbp->vst_uid = sbp->st_uid;
    vsbp->vst_gid = sbp->st_gid;
    vsbp->vst_atime = sbp->st_atimespec.tv_sec;
    vsbp->vst_atimensec = sbp->st_atimespec.tv_nsec;
    vsbp->vst_mtime = sbp->st_mtimespec.tv_sec;
    vsbp->vst_mtimensec = sbp->st_mtimespec.tv_nsec;
    vsbp->vst_ctime = sbp->st_ctimespec.tv_sec;
    vsbp->vst_ctimensec = sbp->st_ctimespec.tv_nsec;
    vsbp->vst_birthtime = sbp->st_birthtimespec.tv_sec;
    vsbp->vst_birthtimensec = sbp->st_birthtimespec.tv_nsec;
    vsbp->vst_size = sbp->st_size;
    vsbp->vst_blocks = sbp->st_blocks;
    vsbp->vst_blksize = sbp->st_blksize;
    vsbp->vst_flags = sbp->st_flags;
    vsbp->vst_gen = sbp->st_gen;
    vsbp->vst_rdev = sbp->st_rdev;
    vsbp->vst_qspare[0] = sbp->st_qspare[0];
    vsbp->vst_qspare[1] = sbp->st_qspare[1];
}

int
fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, __unused boolean_t check_fsgetpath)
{
    vfs_context_t context;
    struct stat64 sb;
    int error = 0;

    struct vnode_attr *vap;

    bzero(&sb, sizeof(struct stat64));
    context = vfs_context_create((vfs_context_t)0);

    if (!error) {
        if (_vn_stat != NULL) {
            error = vn_stat(vp, &sb, NULL, 1, 0, context);
            munge_vinfo_stat(&sb, &vinfo->vi_stat);
        }
    }

    (void)vfs_context_rele(context);

    if (error != 0) {
        goto out;
    }

    if (_dead_mountp != NULL && vnode_mount(vp) != dead_mountp) {
        vinfo->vi_fsid = vp->v_mount->mnt_vfsstat.f_fsid;
    } else {
        vinfo->vi_fsid.val[0] = 0;
        vinfo->vi_fsid.val[1] = 0;
    }
    vinfo->vi_type = vnode_vtype(vp);

out:
    return error;
}


/*
 * Returns the fdp pointer for the specified
 * process.
 */
static inline volatile struct filedesc *
proc_fdp(proc_t p)
{
    return (volatile struct filedesc *)&p->p_fd;
}

int
fill_fileinfo(struct fileproc * fp, proc_t p, int fd, struct proc_fileinfo * fi)
{
    uint32_t openflags = 0;
    uint32_t status = 0;
    off_t offset = 0;
    int32_t type = 0;
    uint32_t guardflags = 0;

    if (fp != FILEPROC_NULL) {
        openflags = fp->fp_glob->fg_flag;
        offset = fp->fp_glob->fg_offset;
        type = FILEGLOB_DTYPE(fp->fp_glob);

        if (os_ref_get_count_raw(&fp->fp_glob->fg_count) > 1) {
            status |= PROC_FP_SHARED;
        }

        if (p != PROC_NULL) {
            volatile struct filedesc *fdp = NULL;

            proc_fdlock(p);
            fdp = proc_fdp(p);
            if (fdp != NULL) {
                if (fp->fp_flags & FP_CLOEXEC) {
                    status |= PROC_FP_CLEXEC;
                }
                if (fp->fp_flags & FP_CLOFORK) {
                    status |= PROC_FP_CLFORK;
                }
#if DEBUG
            } else {
                proc_fdunlock(p);
                return (EBADF);
#endif
            }
            proc_fdunlock(p);
        }

        if (fp->fp_guard_attrs != 0) {
            status |= PROC_FP_GUARDED;
            if (fp->fp_guard_attrs & GUARD_CLOSE) {
                guardflags |= PROC_FI_GUARD_CLOSE;
            }
            if (fp->fp_guard_attrs & GUARD_DUP) {
                guardflags |= PROC_FI_GUARD_DUP;
            }
            if (fp->fp_guard_attrs & GUARD_SOCKET_IPC) {
                guardflags |= PROC_FI_GUARD_SOCKET_IPC;
            }
            if (fp->fp_guard_attrs & GUARD_FILEPORT) {
                guardflags |= PROC_FI_GUARD_FILEPORT;
            }
        }

    }
    bzero(fi, sizeof(struct proc_fileinfo));
    fi->fi_openflags = openflags;
    fi->fi_status = status;
    fi->fi_offset = offset;
    fi->fi_type = type;
    fi->fi_guardflags = guardflags;

    return (0);
}

