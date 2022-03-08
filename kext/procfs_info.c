#include <string.h>
#include <libkern/OSMalloc.h>
#include <os/refcnt.h>
#include <sys/cdefs.h>
#include <sys/file_internal.h>
#include <sys/kauth.h>
#include <sys/mount_internal.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vnode.h>

#include <libklookup/klookup.h>

#include "procfs_internal.h"

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

int
procfs_proc_pidshortbsdinfo(proc_t p, struct proc_bsdshortinfo * pbsd_shortp, int zombie)
{
    bzero(pbsd_shortp, sizeof(struct proc_bsdshortinfo));
    pbsd_shortp->pbsi_pid = proc_pid(p);
    pbsd_shortp->pbsi_ppid = proc_ppid(p);
    pbsd_shortp->pbsi_pgid = proc_pgrpid(p);
    // FIXME:
    pbsd_shortp->pbsi_status = p->p_stat;
    // FIXME:
    bcopy(&p->p_comm, &pbsd_shortp->pbsi_comm[0], MAXCOMLEN);
    pbsd_shortp->pbsi_comm[MAXCOMLEN - 1] = '\0';

    pbsd_shortp->pbsi_flags = 0;
    // FIXME:
    if ((p->p_flag & P_SYSTEM) == P_SYSTEM) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_SYSTEM;
    }
    // FIXME:
    if ((p->p_lflag & P_LTRACED) == P_LTRACED) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_TRACED;
    }
    if (proc_exiting(p) != 0) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_INEXIT;
    }
    // FIXME:
    if ((p->p_lflag & P_LPPWAIT) == P_LPPWAIT) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PPWAIT;
    }
    if (proc_is64bit(p) != 0) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_LP64;
    }
    // FIXME:
    if ((p->p_flag & P_CONTROLT) == P_CONTROLT) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_CONTROLT;
    }
    // FIXME:
    if ((p->p_flag & P_THCWD) == P_THCWD) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_THCWD;
    }
    if (_proc_issetugid(p) != 0)  {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PSUGID;
    }
    // FIXME:
    if ((p->p_flag & P_EXEC) == P_EXEC) {
        pbsd_shortp->pbsi_flags |= PROC_FLAG_EXEC;
    }
    // FIXME:
    if ((p->p_flag & P_DELAYIDLESLEEP) == P_DELAYIDLESLEEP) {
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
    }
    ;

    switch (PROC_ACTION_STATE(p)) {
    case P_PCTHROTTLE:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PA_THROTTLE;
        break;
    case P_PCSUSP:
        pbsd_shortp->pbsi_flags |= PROC_FLAG_PA_SUSP;
        break;
    }
    ;

    /* if process is a zombie skip bg state */
    // FIXME:
    if ((zombie == 0) && (p->p_stat != SZOMB) && (_proc_task(p) != TASK_NULL)) {
        _proc_get_darwinbgstate(_proc_task(p), &pbsd_shortp->pbsi_flags);
    }

    kauth_cred_t my_cred = kauth_cred_proc_ref(p);
    pbsd_shortp->pbsi_uid = kauth_cred_getuid(my_cred);
    pbsd_shortp->pbsi_gid = kauth_cred_getgid(my_cred);
    pbsd_shortp->pbsi_ruid = kauth_cred_getruid(my_cred);
    pbsd_shortp->pbsi_rgid = kauth_cred_getrgid(my_cred);
    pbsd_shortp->pbsi_svuid = kauth_cred_getsvuid(my_cred);
    pbsd_shortp->pbsi_svgid = kauth_cred_getsvgid(my_cred);
    kauth_cred_unref(&my_cred);

    return 0;
}

int
procfs_proc_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo)
{
    task_t task;

    task = _proc_task(p);

    bzero(ptinfo, sizeof(struct proc_taskinfo));
    _fill_taskprocinfo(task, (struct proc_taskinfo_internal *)ptinfo);

    return 0;
}

int
procfs_proc_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo)
{
    int error = 0;
    uint64_t threadaddr = (uint64_t)arg;

    bzero(pthinfo, sizeof(struct proc_threadinfo));

    error = _fill_taskthreadinfo(p->task, threadaddr, thuniqueid, (struct proc_threadinfo_internal *)pthinfo, NULL, NULL);
    if (error) {
        return ESRCH;
    } else {
        return 0;
    }
}

#if 0
static inline struct guarded_fileproc *
FP_TO_GFP(struct fileproc *fp)
{
    struct guarded_fileproc *gfp =
        __container_of(fp, struct guarded_fileproc, gf_fileproc);

    zone_require(gfp_zone, gfp);
    return gfp;
}

/*
 * Expected use pattern:
 *
 * if (fp_isguarded(fp, GUARD_CLOSE)) {
 *      error = fp_guard_exception(p, fd, fp, kGUARD_EXC_CLOSE);
 *      proc_fdunlock(p);
 *      return error;
 * }
 *
 * Passing `0` to `attrs` returns whether the fp is guarded at all.
 */

int
fp_isguarded(struct fileproc *fp, u_int attrs)
{
    if (FILEPROC_TYPE(fp) == FTYPE_GUARDED) {
        return (attrs & FP_TO_GFP(fp)->gf_attrs) == attrs;
    }
    return 0;
}

void
guarded_fileproc_free(struct fileproc *fp)
{
    struct guarded_fileproc *gfp = FP_TO_GFP(fp);
    zfree(gfp_zone, gfp);
}
#endif

// FIXME:
void
procfs_fill_fileinfo(struct fileproc * fp, proc_t proc, int fd, struct proc_fileinfo * fi)
{
    fi->fi_openflags = fp->fp_glob->fg_flag;
    fi->fi_status = 0;
    fi->fi_offset = fp->fp_glob->fg_offset;
    fi->fi_type = FILEGLOB_DTYPE(fp->fp_glob);
    if (os_ref_get_count_raw(&fp->fp_glob->fg_count) > 1) {
        fi->fi_status |= PROC_FP_SHARED;
    }
    if (proc != PROC_NULL) {
        if ((FDFLAGS_GET(proc, fd) & UF_EXCLOSE) != 0) {
            fi->fi_status |= PROC_FP_CLEXEC;
        }
        if ((FDFLAGS_GET(proc, fd) & UF_FORKCLOSE) != 0) {
            fi->fi_status |= PROC_FP_CLFORK;
        }
    }
#if 0
    if (fp_isguarded(fp, 0)) {
        fi->fi_status |= PROC_FP_GUARDED;
        fi->fi_guardflags = 0;
        if (fp_isguarded(fp, GUARD_CLOSE)) {
            fi->fi_guardflags |= PROC_FI_GUARD_CLOSE;
        }
        if (fp_isguarded(fp, GUARD_DUP)) {
            fi->fi_guardflags |= PROC_FI_GUARD_DUP;
        }
        if (fp_isguarded(fp, GUARD_SOCKET_IPC)) {
            fi->fi_guardflags |= PROC_FI_GUARD_SOCKET_IPC;
        }
        if (fp_isguarded(fp, GUARD_FILEPORT)) {
            fi->fi_guardflags |= PROC_FI_GUARD_FILEPORT;
        }
    }
#endif
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
procfs_fill_vnodeinfo(vnode_t vp, struct vnode_info *vi, vfs_context_t ctx)
{
    struct mount * _dead_mountp = SymbolLookup("_dead_mountp");

    int error = 0;
    struct stat64 sb;

    // struct vnode_info:
    bzero(&sb, sizeof(struct stat64));

    if (!error) {
        error = _vn_stat(vp, &sb, NULL, 1, 0, ctx);
        munge_vinfo_stat(&sb, &vi->vi_stat);
    }

    if (error != 0) {
        goto out;
    }

    vi->vi_pad = 0; // ???
    vi->vi_type = vnode_vfstypenum(vp);

    if (vp->v_mount != _dead_mountp) {
        vi->vi_fsid = vnode_mount(vp)->mnt_vfsstat.f_fsid;
    } else {
        vi->vi_fsid.val[0] = 0;
        vi->vi_fsid.val[1] = 0;
    }

out:
    return error;
}
