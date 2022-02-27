#include <sys/cdefs.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/guarded.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/proc_internal.h>
#include <sys/sysproto.h>
#include <sys/types.h>
#include <sys/vnode.h>

#include <libklookup/klookup.h>

static int(*_vn_stat)(struct vnode *vp, void * sb, kauth_filesec_t *xsec, int isstat64, int needsrealdev, vfs_context_t ctx) = NULL;
static uint64_t(*_vnode_get_va_fsid)(struct vnode_attr *vap) = NULL;

__options_decl(fileproc_vflags_t, unsigned int, {
    FPV_NONE        = 0,
    FPV_DRAIN       = 0x01,
});

/*
 * Kernel descriptor table.
 * One entry for each open kernel vnode and socket.
 */
struct fileproc {
    unsigned int fp_flags;
    _Atomic fileproc_vflags_t fp_vflags;
    os_refcnt_t fp_iocount;
    struct fileglob * fp_glob;
    void *fp_wset;
};


#define FILEPROC_NULL (struct fileproc *)0

#define FP_INSELECT     0x0004
#define FP_AIOISSUED    0x0080
#define FP_SELCONFLICT  0x0200  /* select conflict on an individual fp */

/* squeeze a "type" value into the upper flag bits */

#define _FP_TYPESHIFT   24
#define FP_TYPEMASK     (0x7 << _FP_TYPESHIFT)  /* 8 "types" of fileproc */

#define FILEPROC_TYPE(fp)       ((fp)->fp_flags & FP_TYPEMASK)

typedef enum {
    FTYPE_SIMPLE    = 0,
    FTYPE_GUARDED   = (1 << _FP_TYPESHIFT)
} fileproc_type_t;

/* file types */
typedef enum {
    DTYPE_VNODE     = 1,    /* file */
    DTYPE_SOCKET,           /* communications endpoint */
    DTYPE_PSXSHM,           /* POSIX Shared memory */
    DTYPE_PSXSEM,           /* POSIX Semaphores */
    DTYPE_KQUEUE,           /* kqueue */
    DTYPE_PIPE,             /* pipe */
    DTYPE_FSEVENTS,         /* fsevents */
    DTYPE_ATALK,            /* (obsolete) */
    DTYPE_NETPOLICY,        /* networking policy */
} file_type_t;

struct fileops {
    file_type_t     fo_type;        /* descriptor type */
    int (*fo_read)      (struct fileproc *fp, struct uio *uio,
        int flags, vfs_context_t ctx);
    int (*fo_write)     (struct fileproc *fp, struct uio *uio,
        int flags, vfs_context_t ctx);
#define FOF_OFFSET      0x00000001      /* offset supplied to vn_write */
#define FOF_PCRED       0x00000002      /* cred from proc, not current thread */
    int (*fo_ioctl)(struct fileproc *fp, u_long com,
        caddr_t data, vfs_context_t ctx);
    int (*fo_select)    (struct fileproc *fp, int which,
        void *wql, vfs_context_t ctx);
    int (*fo_close)     (struct fileglob *fg, vfs_context_t ctx);
    int (*fo_kqfilter)  (struct fileproc *fp, struct knote *, struct kevent_qos_s *);
    int (*fo_drain)     (struct fileproc *fp, vfs_context_t ctx);
};

struct fileglob {
    LIST_ENTRY(fileglob) f_msglist;     /* list of files in unix messages */
    uint32_t             fg_flag;       /* (atomic) see fcntl.h */
    os_ref_atomic_t      fg_count;      /* reference count */
    uint32_t             fg_msgcount;   /* references from message queue */
    int32_t              fg_lflags;     /* file global flags */
    kauth_cred_t         fg_cred;       /* credentials associated with descriptor */
    const struct fileops *fg_ops;
    off_t                fg_offset;
    void                *fg_data;       /* vnode or socket or SHM or semaphore */
    void                *fg_vn_data;    /* Per fd vnode data, used for directories */
    lck_mtx_t            fg_lock;
    struct label        *fg_label;      /* JMM - use the one in the cred? */
};

#if 0
void
procfs_fill_fileinfo(struct fileproc * fp, proc_t proc, int fd, struct proc_fileinfo * fproc)
{
    //_fp_isguarded = SymbolLookup("_fp_isguarded"); // can't be resolved

    fproc->fi_openflags = fp->fp_glob->fg_flag;
    fproc->fi_status = 0;
    fproc->fi_offset = fp->fp_glob->fg_offset;
    fproc->fi_type = FILEGLOB_DTYPE(fp->fp_glob);
    if (os_ref_get_count_raw(&fp->fp_glob->fg_count) > 1) {
        fproc->fi_status |= PROC_FP_SHARED;
    }
    if (proc != PROC_NULL) {
        if ((FDFLAGS_GET(proc, fd) & UF_EXCLOSE) != 0) {
            fproc->fi_status |= PROC_FP_CLEXEC;
        }
        if ((FDFLAGS_GET(proc, fd) & UF_FORKCLOSE) != 0) {
            fproc->fi_status |= PROC_FP_CLFORK;
        }
    }

    if (_fp_isguarded(fp, 0)) {
        fproc->fi_status |= PROC_FP_GUARDED;
        fproc->fi_guardflags = 0;
        if (_fp_isguarded(fp, GUARD_CLOSE)) {
            fproc->fi_guardflags |= PROC_FI_GUARD_CLOSE;
        }
        if (_fp_isguarded(fp, GUARD_DUP)) {
            fproc->fi_guardflags |= PROC_FI_GUARD_DUP;
        }
        if (_fp_isguarded(fp, GUARD_SOCKET_IPC)) {
            fproc->fi_guardflags |= PROC_FI_GUARD_SOCKET_IPC;
        }
        if (_fp_isguarded(fp, GUARD_FILEPORT)) {
            fproc->fi_guardflags |= PROC_FI_GUARD_FILEPORT;
        }
    }
}
#endif

void
procfs_fill_fileinfo(struct fileproc * fp, proc_t proc, int fd, struct proc_fileinfo * fproc) {
    uint32_t        oflags;
    uint32_t        status;
    off_t           offset;
    int32_t         type;
    uint32_t        gflags;

    oflags = fp->fp_glob->fg_flag;
    status = 0;
    offset = fp->fp_glob->fg_offset;
    type   = ((const file_type_t)((fp->fp_glob)->fg_ops->fo_type));
    gflags = 0;

    if (proc != PROC_NULL) {
        if ((FDFLAGS_GET(proc, fd) & UF_EXCLOSE) != 0) {
            status |= PROC_FP_CLEXEC;
        }
        if ((FDFLAGS_GET(proc, fd) & UF_FORKCLOSE) != 0) {
            status |= PROC_FP_CLFORK;
        }
    }

    if (FILEPROC_TYPE(fp) == FTYPE_GUARDED) {
        status |= PROC_FP_GUARDED;

#if 0
        if (/*???*/) {
            gflags |= PROC_FI_GUARD_CLOSE;
        }
        if (/*???*/) {
            gflags |= PROC_FI_GUARD_DUP;
        }
        if (/*???*/) {
            gflags |= PROC_FI_GUARD_SOCKET_IPC;
        }
        if (/*???*/) {
            gflags |= PROC_FI_GUARD_FILEPORT;
        }
#endif

    }

    fproc->fi_openflags   = oflags;
    fproc->fi_status      = status;
    fproc->fi_offset      = offset;
    fproc->fi_type        = type;
    fproc->fi_guardflags  = gflags;
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
procfs_fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo)
{
    struct mount * dead_mountp = SymbolLookup("_dead_mountp");
    _vnode_get_va_fsid = SymbolLookup("_vnode_get_va_fsid");
    _vn_stat = SymbolLookup("_vn_stat");

    vfs_context_t context;
    struct stat64 sb;
    struct vnode_attr vap;
    int error = 0;

    bzero(&sb, sizeof(struct stat64));
    context = vfs_context_create((vfs_context_t)0);

    int vattr = vnode_getattr(vp, &vap, context);
    fsid_t *fsidp = _vnode_get_va_fsid(vattr);
    fsid_t fsid = *fsidp;

    if (!error) {
        error = _vn_stat(vp, &sb, NULL, 1, 0, context);
        munge_vinfo_stat(&sb, &vinfo->vi_stat);
    }

    (void)vfs_context_rele(context);
    if (error != 0) {
        goto out;
    }

    if (vnode_mount(vp) != dead_mountp) {
        vinfo->vi_fsid = fsid;
    } else {
        vinfo->vi_fsid.val[0] = 0;
        vinfo->vi_fsid.val[1] = 0;
    }
    vinfo->vi_type = vnode_vfstypenum(vp);
out:
    return error;
}
