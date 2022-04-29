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
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/vnode_internal.h>

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

    if (proc_issetugid(p) != 0)  {
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
    if ((zombie == 0) && (proc_status != SZOMB) && (proc_task(p) != TASK_NULL)) {
        proc_get_darwinbgstate(proc_task(p), &pbsd_shortp->pbsi_flags);
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
    fill_taskprocinfo(proc_task(p), (struct proc_taskinfo_internal *)ptinfo);

    return 0;
}

/*
 * From bsd/kern/proc_info.c
 */
int
proc_pidthreadinfo(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo)
{
    bzero(pthinfo, sizeof(struct proc_threadinfo));
    fill_taskthreadinfo(proc_task(p), (uint64_t)arg, thuniqueid, (struct proc_threadinfo_internal *)pthinfo, NULL, NULL);

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
        error = vn_stat(vp, &sb, NULL, 1, 0, context);
        munge_vinfo_stat(&sb, &vinfo->vi_stat);
    }

    (void)vfs_context_rele(context);

    if (error != 0) {
        goto out;
    }

    if (vnode_mount(vp) != dead_mountp) {
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

static inline struct guarded_fileproc *
FP_TO_GFP(struct fileproc *fp)
{
#define typeof __typeof

    struct guarded_fileproc *gfp = __container_of(fp, struct guarded_fileproc, gf_fileproc);

#undef typeof

    return gfp;
}

static inline int
fp_isguarded(struct fileproc *fp, u_int attrs)
{
    if (FILEPROC_TYPE(fp) == FTYPE_GUARDED) {
        return (attrs & FP_TO_GFP(fp)->gf_attrs) == attrs;
    }
    return 0;
}

int
fill_fileinfo(struct fileproc * fp, proc_t p, vnode_t vp, int vid, int fd,
              struct proc_fileinfo * fi, vfs_context_t ctx)
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
            struct filedesc *fdp;

            /*
             * FIXME: p->p_fd is always NULL because the
             * filedesc structure is not included in the
             * public KPI.
             *
             * We'll have to find a way around this problem
             * to get the p->p_fd->fd_ofileflags array.
             *
             * The user mode fd integer is an indice into
             * this array (0 - stdin, 1 - stdout, 2 - stderr).
             */
            proc_fdlock(p);
            if ((fdp = p->p_fd) != NULL) {
                if ((FDFLAGS_GET(p, fd) & UF_EXCLOSE) != 0) {
                    status |= PROC_FP_CLEXEC;
                }
                if ((FDFLAGS_GET(p, fd) & UF_FORKCLOSE) != 0) {
                    status |= PROC_FP_CLFORK;
                }
            }
            proc_fdunlock(p);
        }

        if (fp_isguarded(fp, 0)) {
            status |= PROC_FP_GUARDED;
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

    }
    bzero(fi, sizeof(struct proc_fileinfo));
    fi->fi_openflags = openflags;
    fi->fi_status = status;
    fi->fi_offset = offset;
    fi->fi_type = type;
    fi->fi_guardflags = guardflags;

    return (0);
}
