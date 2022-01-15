#include <mach/mach_types.h>

#include <libkern/libkern.h>
#include <libkern/locks.h>

#include <sys/mount.h>

#include "procfs.h"
#include "utils.h"

extern struct vfsops procfs_vfsops;
extern struct vnodeopv_desc procfs_vnodeop_opv_desc;

lck_grp_t *procfs_lck_grp;

static vfstable_t procfs_vfstbl_ref;

static struct vfs_fsentry procfs_vfsentry = {
    &procfs_vfsops,
    ARRAY_SIZE(procfs_vnopv_desc_list),
    procfs_vnopv_desc_list,
    PROCFS_FSTYPENUM,
    PROCFS_NAME,
    PROCFS_VFS_FLAGS,
    {NULL, NULL},
};

kern_return_t procfs_start(kmod_info_t *ki, void *d __unused)
{
    kern_return_t e;
    uuid_string_t uuid;

    LOG("built with Apple LLVM version %s", __clang_version__);

    e = util_vma_uuid(ki->address, uuid);
    kassert(e == 0);
    LOG("kext executable uuid %s", uuid);

    procfs_lck_grp = lck_grp_alloc_init(PROCFS_LCKGRP_NAME, LCK_GRP_ATTR_NULL);
    if (procfs_lck_grp == NULL) {
        LOG_ERR("lck_grp_alloc_init() fail");
        goto out_procfs_lck_grp;
    }
    LOG_DBG("lock group(%s) allocated", PROCFS_LCKGRP_NAME);

    e = vfs_fsadd(&procfs_vfsentry, &procfs_vfstbl_ref);
    if (e != 0) {
        LOG_ERR("vfs_fsadd() failure  errno: %d", e);
        goto out_vfsadd;
    }
    LOG_DBG("%s file system registered", procfs_vfsentry.vfe_fsname);

    LOG("loaded %s version %s build %s (%s)",
        BUNDLEID_S, KEXTVERSION_S, KEXTBUILD_S, __TS__);

out_exit:
    return e;

out_vfsadd:
    lck_grp_free(procfs_lck_grp);

out_procfs_lck_grp:
    e = KERN_FAILURE;
    goto out_exit;
}

kern_return_t procfs_stop(kmod_info_t *ki __unused, void *d __unused)
{
    kern_return_t e;

    /* will fail if any of our volumes(i.e. procfs) mounted */
    e = vfs_fsremove(procfs_vfstbl_ref);
    if (e) {
        LOG_ERR("vfs_fsremove() failure  errno: %d", e);
        goto out_vfs_rm;
    }

    lck_grp_free(procfs_lck_grp);

    util_massert();

    LOG("unloaded %s version %s build %s (%s)",
        BUNDLEID_S, KEXTVERSION_S, KEXTBUILD_S, __TS__);

out_exit:
    return e;

out_vfs_rm:
    e = KERN_FAILURE;
    goto out_exit;
}

kern_return_t _start (kmod_info_t *kinfo, void *data);
kern_return_t _stop (kmod_info_t *kinfo, void *data);

KMOD_EXPLICIT_DECL (BUNDLEID, KEXTBUILD_S, _start, _stop)
  __attribute__ ((visibility ("default")))

__private_extern__ kmod_start_func_t *_realmain = procfs_start;
__private_extern__ kmod_start_func_t *_antimain = procfs_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;

