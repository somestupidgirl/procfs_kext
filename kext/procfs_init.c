#include <mach/kern_return.h>
#include <mach/kmod.h>
#include <mach/mach_types.h>

#include <kern/lock_group.h>

#include <libkern/libkern.h>
#include <libkern/locks.h>
#include <libkern/OSMalloc.h>

#include <sys/filedesc.h>
#include <sys/kernel_types.h>
#include <sys/mount.h>
#include <sys/types.h>

#include "procfs.h"
#include "utils.h"

#pragma mark -
#pragma mark External References

extern int procfs_init(__unused struct vfsconf *vfsconf);
extern void procfs_fini(void);
extern struct vfs_fsentry procfs_vfsentry;
extern vfstable_t procfs_vfs_table_ref;

#pragma mark -
#pragma mark Function Prototypes

kern_return_t procfs_start(kmod_info_t *ki, __unused void *d);
kern_return_t procfs_stop(__unused kmod_info_t *ki, __unused void *d);

#pragma mark -
#pragma mark Start/Stop Routines

kern_return_t
procfs_start(kmod_info_t *ki, __unused void *d)
{
    kern_return_t ret = KERN_SUCCESS;
    uuid_string_t uuid;
    struct vfsconf *vfsc;

    ret = util_vma_uuid(ki->address, uuid);
    kassert(ret == KERN_SUCCESS);
    LOG("kext executable uuid %s", uuid);

    ret = procfs_init(&vfsc);
    if (ret != KERN_SUCCESS) {
        LOG_ERR("procfs_init() fail");
        goto out_error;
    }
    LOG_DBG("lock group(%s) allocated", PROCFS_LCK_GRP_NAME);

    ret = vfs_fsadd(&procfs_vfsentry, &procfs_vfs_table_ref);
    if (ret != KERN_SUCCESS) {
        LOG_ERR("vfs_fsadd() failure  errno: %d", ret);
        procfs_vfs_table_ref = NULL;
        goto out_vfsadd;
    }
    LOG_DBG("%s file system registered", procfs_vfsentry.vfe_fsname);

    LOG("loaded %s version %s build %s (%s)",
        PROCFS_BUNDLEID, PROCFS_VERSION, PROCFS_BUILDNUM, __TS__);

    if (ret == KERN_SUCCESS) {
        goto out_exit;
    } else {
        goto out_error;
    }

out_exit:
    return ret;

out_vfsadd:
    procfs_fini();
    goto out_error;

out_error:
    ret = KERN_FAILURE;
    goto out_exit;
}

kern_return_t procfs_stop(__unused kmod_info_t *ki, __unused void *d)
{
    kern_return_t ret = KERN_SUCCESS;

    ret = vfs_fsremove(procfs_vfs_table_ref);
    if (ret) {
        LOG_ERR("vfs_fsremove() failure  errno: %d", ret);
        goto out_vfs_rm;
    }

    procfs_fini();

    util_massert();

    LOG("unloaded %s version %s build %s (%s)",
        BUNDLEID_S, KEXTVERSION_S, KEXTBUILD_S, __TS__);

out_exit:
    return ret;

out_vfs_rm:
    ret = KERN_FAILURE;
    goto out_exit;
}

KMOD_EXPLICIT_DECL (PROCFS_BUNDLEID, PROCFS_BUILDNUM, procfs_start, procfs_stop)
  __attribute__ ((visibility ("default")))

__private_extern__ kmod_start_func_t *_realmain = procfs_start;
__private_extern__ kmod_start_func_t *_antimain = procfs_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
