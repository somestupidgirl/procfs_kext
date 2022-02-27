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
#include "symbols.h"

__options_decl(fileproc_vflags_t, unsigned int, {
	FPV_NONE        = 0,
	FPV_DRAIN       = 0x01,
});

/*
 * Kernel descriptor table.
 * One entry for each open kernel vnode and socket.
 */
struct fileproc {
	unsigned int               fp_flags;
	_Atomic fileproc_vflags_t  fp_vflags;
	os_refcnt_t                fp_iocount;
	struct fileglob *          fp_glob;
	void                      *fp_wset;
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
};

void
_fill_fileinfo(struct fileproc *fp, proc_t proc, int fd, struct proc_fileinfo *proc_finfo)
{
	proc_finfo->fi_openflags = _f_flag;
	proc_finfo->fi_status = 0;
	proc_finfo->fi_offset = _f_offset;
	proc_finfo->fi_type = _f_type;

	if (os_ref_get_count_raw(&fp->fp_glob->fg_count) > 1) {
		proc_finfo->fi_status |= PROC_FP_SHARED;
	}

	if (proc != PROC_NULL) {
		if ((FDFLAGS_GET(proc, fd) & UF_EXCLOSE) != 0) {
			proc_finfo->fi_status |= PROC_FP_CLEXEC;
		}

		if ((FDFLAGS_GET(proc, fd) & UF_FORKCLOSE) != 0) {
			proc_finfo->fi_status |= PROC_FP_CLFORK;
		}
	}
#if 0
	if (_fp_isguarded(fp, 0)) {
		proc_finfo->fi_status |= PROC_FP_GUARDED;
		proc_finfo->fi_guardflags = 0;

		if (_fp_isguarded(fp, GUARD_CLOSE)) {
			proc_finfo->fi_guardflags |= PROC_FI_GUARD_CLOSE;
		}

		if (_fp_isguarded(fp, GUARD_DUP)) {
			proc_finfo->fi_guardflags |= PROC_FI_GUARD_DUP;
		}

		if (_fp_isguarded(fp, GUARD_SOCKET_IPC)) {
			proc_finfo->fi_guardflags |= PROC_FI_GUARD_SOCKET_IPC;
		}

		if (_fp_isguarded(fp, GUARD_FILEPORT)) {
			proc_finfo->fi_guardflags |= PROC_FI_GUARD_FILEPORT;
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
_fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, __unused boolean_t check_fsgetpath)
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

