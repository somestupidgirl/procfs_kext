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
procfs_fill_vnodeinfo(vnode_t vp, struct vnode_info *vinfo, __unused boolean_t check_fsgetpath)
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
