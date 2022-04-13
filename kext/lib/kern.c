#include <os/log.h>
#include <os/refcnt.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/mount_internal.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/vnode_internal.h>

#include "helpers.h"
#include "symbols.h"

/*
 * From bsd/kern/proc_info.c
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

    if (_proc_issetugid(p) != 0)  {
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
    // FIXME:
    if ((zombie == 0) && (proc_status != SZOMB) && (_proc_task(p) != TASK_NULL)) {
        _proc_get_darwinbgstate(_proc_task(p), &pbsd_shortp->pbsi_flags);
    }

    return 0;
}

/*
 * From bsd/kern/proc_info.c
 */
int
proc_pidtaskinfo(proc_t p, struct proc_taskinfo * ptinfo)
{
    bzero(ptinfo, sizeof(struct proc_taskinfo));
    _fill_taskprocinfo(_proc_task(p), (struct proc_taskinfo_internal *)ptinfo);

    return 0;
}

/*
 * From bsd/kern/proc_info.c
 */
int
proc_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo)
{
    bzero(pthinfo, sizeof(struct proc_threadinfo));
    _fill_taskthreadinfo(_proc_task(p), (uint64_t)arg, thuniqueid, (struct proc_threadinfo_internal *)pthinfo, NULL, NULL);

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
        error = _vn_stat(vp, &sb, NULL, 1, 0, context);
        munge_vinfo_stat(&sb, &vinfo->vi_stat);
    }

    (void)vfs_context_rele(context);

    if (error != 0) {
        goto out;
    }

    if (vnode_mount(vp) != *_dead_mountp) {
        vinfo->vi_fsid = vp->v_mount->mnt_vfsstat.f_fsid;
    } else {
        vinfo->vi_fsid.val[0] = 0;
        vinfo->vi_fsid.val[1] = 0;
    }
    vinfo->vi_type = vnode_vtype(vp);

out:
    return error;
}

void
fill_fileinfo(struct fileproc * fp, proc_t proc, int fd, struct proc_fileinfo * fi)
{   
    uint32_t openflags = 0;
    uint32_t status = 0;
    off_t offset = 0;
    int32_t type = 0;
    uint32_t guardflags = 0;

    openflags = fp->fp_glob->fg_flag;
    offset = fp->fp_glob->fg_offset;
    type = FILEGLOB_DTYPE(fp->fp_glob);

    if (os_ref_get_count_raw(&fp->fp_glob->fg_count) > 1) {
        status |= PROC_FP_SHARED;
    }

    if (proc != PROC_NULL) {
        if ((FDFLAGS_GET(proc, fd) & UF_EXCLOSE) != 0) {
            status |= PROC_FP_CLEXEC;
        }
        if ((FDFLAGS_GET(proc, fd) & UF_FORKCLOSE) != 0) {
            status |= PROC_FP_CLFORK;
        }
    }

#if 0
    if (fp_isguarded(fp, 0)) {
        fproc->fi_status |= PROC_FP_GUARDED;
        fproc->fi_guardflags = 0;
        if (fp_isguarded(fp, GUARD_CLOSE)) {
            guardflags |= PROC_FI_GUARD_CLOSE;
        }
        if (fp_isguarded(fp, GUARD_DUP)) {
            guardflags |= PROC_FI_GUARD_DUP;
        }
        if (fp_isguarded(fp, GUARD_SOCKET_IPC)) {
            guardflags |= PROC_FI_GUARD_SOCKET_IPC;
        }
        if (fp_isguarded(fp, GUARD_FILEPORT)) {
            guardflags |= PROC_FI_GUARD_FILEPORT;
        }
    }
#endif

    bzero(fi, sizeof(struct proc_fileinfo));
    fi->fi_openflags = openflags;
    fi->fi_status = status;
    fi->fi_offset = offset;
    fi->fi_type = type;
    fi->fi_guardflags = guardflags;
}

/*
 * fp_getfvpandvid
 *
 * Description: Get fileproc, vnode pointer, and vid for a given fd from the
 *      per process open file table of the specified process, and if
 *      successful, increment the f_iocount
 *
 * Parameters:  p               Process in which fd lives
 *      fd              fd to get information for
 *      resultfp            Pointer to result fileproc
 *                      pointer area, or 0 if none
 *      resultvp            Pointer to result vnode pointer
 *                      area, or 0 if none
 *      vidp                Pointer to resuld vid area
 *
 * Returns: 0               Success
 *      EBADF               Bad file descriptor
 *      ENOTSUP             fd does not refer to a vnode
 *
 * Implicit returns:
 *      *resultfp (modified)        Fileproc pointer
 *      *resultvp (modified)        vnode pointer
 *      *vidp               vid value
 *
 * Notes:   The resultfp and resultvp fields are optional, and may be
 *      independently specified as NULL to skip returning information
 *
 * Locks:   Internally takes and releases proc_fdlock
 */
int
fp_getfvpandvid(proc_t p, int fd, struct fileproc **resultfp, struct vnode **resultvp, uint32_t *vidp)
{
    struct fileproc *fp;
    struct filedesc *fdp;

    vnode_t vp;
    thread_t th;
    uthread_t uth;

    th = current_thread();
    uth = (uthread_t)_get_bsdthread_info(th); // returns th->uthread

    // Sets vp to uth->uu_cdir for fdcopy(p, vp) since
    // we don't have direct access to struct uthread.
    _bsd_threadcdir(uth, vp, vidp);

    _proc_fdlock_spin(p);

    if (p->p_fd == NULL) {
        p->p_fd = _fdcopy(p, vp);
    }

    fdp = p->p_fd;

    if (fdp == NULL) {
        _proc_fdunlock(p);
        return (EBADF);
    }

    if (fd < 0) {
        _proc_fdunlock(p);
        return (EBADF);
    }

    if (fd >= fdp->fd_nfiles) {
        _proc_fdunlock(p);
        return (EBADF);
    }

    if ((fp = fdp->fd_ofiles[fd]) == NULL) {
        _proc_fdunlock(p);
        return (EBADF);
    }

    if (fdp->fd_ofileflags[fd] & UF_RESERVED) {
        _proc_fdunlock(p);
        return (EBADF);
    }

    if (FILEGLOB_DTYPE(fp->fp_glob) != DTYPE_VNODE) {
        _proc_fdunlock(p);
        return (ENOTSUP);
    }

    fp->fp_glob->fg_count++;

    if (resultfp) {
        *resultfp = fp;
    }

    if (resultvp) {
        *resultvp = (struct vnode *)fp->fp_glob->fg_data;
    }

    if (vidp) {
        *vidp = (uint32_t)vnode_vid((struct vnode *)fp->fp_glob->fg_data);
    }
    _proc_fdunlock(p);

    return (0);
}

/*
 * fp_getfsock
 *
 * Description: Get fileproc and socket pointer for a given fd from the
 *      per process open file table of the specified process, and if
 *      successful, increment the f_iocount
 *
 * Parameters:  p               Process in which fd lives
 *      fd              fd to get information for
 *      resultfp            Pointer to result fileproc
 *                      pointer area, or 0 if none
 *      results             Pointer to result socket
 *                      pointer area, or 0 if none
 *
 * Returns: EBADF           The file descriptor is invalid
 *      EOPNOTSUPP      The file descriptor is not a socket
 *      0           Success
 *
 * Implicit returns:
 *      *resultfp (modified)        Fileproc pointer
 *      *results (modified)     socket pointer
 *
 * Notes:   EOPNOTSUPP should probably be ENOTSOCK; this function is only
 *      ever called from accept1().
 */
int
fp_getfsock(proc_t p, int fd, struct fileproc **resultfp, socket_t *results)
{
    struct filedesc *fdp = p->p_fd;
    struct fileproc *fp;

    _proc_fdlock_spin(p);
    if (fd < 0 || fd >= fdp->fd_nfiles ||
            (fp = fdp->fd_ofiles[fd]) == NULL ||
            (fdp->fd_ofileflags[fd] & UF_RESERVED)) {
        _proc_fdunlock(p);
        return (EBADF);
    }
    if (FILEGLOB_DTYPE(fp->fp_glob) != DTYPE_SOCKET) {
        _proc_fdunlock(p);
        return(EOPNOTSUPP);
    }

    fp->fp_glob->fg_count++;

    if (resultfp) {
        *resultfp = fp;
    }

    if (results) {
        *results = (socket_t)fp->fp_glob->fg_data;
    }
    _proc_fdunlock(p);

    return (0);
}

/*
 * fp_drop
 *
 * Description: Drop the I/O reference previously taken by calling fp_lookup
 *      et. al.
 *
 * Parameters:  p               Process in which the fd lives
 *      fd              fd associated with the fileproc
 *      fp              fileproc on which to set the
 *                      flag and drop the reference
 *      locked              flag to internally take and
 *                      drop proc_fdlock if it is not
 *                      already held by the caller
 *
 * Returns: 0               Success
 *      EBADF               Bad file descriptor
 *
 * Locks:   This function internally takes and drops the proc_fdlock for
 *      the supplied process if 'locked' is non-zero, and assumes that
 *      the caller already holds this lock if 'locked' is non-zero.
 *
 * Notes:   The fileproc must correspond to the fd in the supplied proc
 */
int
fp_drop(proc_t p, int fd, struct fileproc *fp, int locked)
{
    struct filedesc *fdp = p->p_fd;
    int needwakeup = 0;

    if (!locked) {
        _proc_fdlock_spin(p);
    }

    if ((fp == FILEPROC_NULL) && (fd < 0 || fd >= fdp->fd_nfiles ||
        (fp = fdp->fd_ofiles[fd]) == NULL ||
        ((fdp->fd_ofileflags[fd] & UF_RESERVED) &&
        !(fdp->fd_ofileflags[fd] & UF_CLOSING)))) {
        if (!locked) {
            _proc_fdunlock(p);
        }
        return EBADF;
    }

    if (1 == os_ref_release_locked(&fp->fp_iocount)) {
        if (fp->fp_flags & FP_SELCONFLICT) {
            fp->fp_flags &= ~FP_SELCONFLICT;
        }

        if (p->p_fpdrainwait) {
            p->p_fpdrainwait = 0;
            needwakeup = 1;
        }
    }

    if (!locked) {
        _proc_fdunlock(p);
    }

    if (needwakeup) {
        wakeup(&p->p_fpdrainwait);
    }

    return 0;
}