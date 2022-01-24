#include <mach/mach_types.h>

#include <libkern/libkern.h>
#include <libkern/locks.h>

#include <sys/mount.h>

#include "procfs.h"

kern_return_t _start (kmod_info_t *kinfo, void *data);
kern_return_t _stop (kmod_info_t *kinfo, void *data);

extern struct vfsops procfs_vfsops;
extern struct vnodeopv_desc procfs_vnodeop_opv_desc;

lck_grp_t *procfs_lck_grp;

static vfstable_t procfs_vfstbl_ref;

static struct vfs_fsentry procfs_vfsentry = {
    &procfs_vfsops,
    sizeof (procfs_vnopv_desc_list) / sizeof (procfs_vnopv_desc_list[PROCFS_FSTYPENUM]),
    procfs_vnopv_desc_list,
    0,
    PROCFS_NAME,
    PROCFS_VFS_FLAGS,
    {NULL, NULL},
};

kern_return_t
procfs_start (kmod_info_t *kinfo, void *data)
{
  kern_return_t err;
  log("starting");
  log_debug("built with Apple LLVM %s", __clang_version__);

  procfs_lck_grp = lck_grp_alloc_init (PROCFS_LCK_GRP_NAME, LCK_GRP_ATTR_NULL);
  if (procfs_lck_grp == NULL)
    {
      log ("failed to allocate lock group");
      return KERN_FAILURE;
    }

  err = vfs_fsadd (&procfs_vfsentry, &procfs_vfstbl_ref);
  if (err != 0)
    {
      log ("failed to register procfs filesystem (errno %d)", err);
      lck_grp_free (procfs_lck_grp);
      return KERN_FAILURE;
    }
  log_debug ("successfully registered procfs filesystem");
  return KERN_SUCCESS;
}

kern_return_t
procfs_stop (kmod_info_t *kinfo, void *data)
{
  kern_return_t err;
  log ("stopping");

  err = vfs_fsremove (procfs_vfstbl_ref);
  if (err != 0)
    {
      log ("failed to remove filesystem (errno %d)", err);
      return KERN_FAILURE;
    }

  lck_grp_free (procfs_lck_grp);
#ifdef DEBUG
  kmemassert ();
#endif
  return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL (PROCFS_KEXTBUNDLE, PROCFS_KEXTBUILD, _start, _stop)
  __attribute__ ((visibility ("default")))

__private_extern__ kmod_start_func_t *_realmain = procfs_start;
__private_extern__ kmod_start_func_t *_antimain = procfs_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
