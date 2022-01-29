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

kern_return_t procfs_start(__unused kmod_info_t *kinfo, __unused void *data);
kern_return_t procfs_stop(__unused kmod_info_t *kinfo, __unused void *data);

#pragma mark -
#pragma mark Start/Stop Routines

kern_return_t
procfs_start(__unused kmod_info_t *kinfo, __unused void *data)
{
    int ret;
    uuid_string_t uuid;
    struct vfsconf *vfsconf;

    ret = util_vma_uuid(kinfo->address, uuid);
    kassert(ret == 0);
    LOG("kext executable uuid %s", uuid);

    ret = procfs_init(vfsconf);
    if (ret == 0) {
        LOG_DBG("lock group(%s) allocated", PROCFS_LCK_GRP_NAME);
    } else {
        LOG_ERR("lck_grp_alloc_init() fail");
        goto error;
    }

    ret = vfs_fsadd(&procfs_vfsentry, &procfs_vfs_table_ref);
    if (ret == 0) {
        LOG_DBG("%s file system registered", procfs_vfsentry.vfe_fsname);
    } else {
        LOG_ERR("vfs_fsadd() failure  errno: %d", ret);
        procfs_vfs_table_ref = NULL;
        goto error;
    }

    LOG("loaded %s version %s build %s (%s)",
        PROCFS_BUNDLEID, PROCFS_VERSION, PROCFS_BUILDNUM, __TS__);

    return KERN_SUCCESS;

error:
    if (procfs_vfs_table_ref) {
        (void)vfs_fsremove(procfs_vfs_table_ref);
    }
    procfs_fini();

    return KERN_FAILURE;
}

kern_return_t
procfs_stop(__unused kmod_info_t *kinfo, __unused void *data)
{
    int ret;

    ret = vfs_fsremove(procfs_vfs_table_ref);
    if (ret != 0) {
        return KERN_FAILURE;
    }
    procfs_fini();

    return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL (PROCFS_BUNDLEID, PROCFS_BUILDNUM, procfs_start, procfs_stop)
  __attribute__ ((visibility ("default")))

__private_extern__ kmod_start_func_t *_realmain = procfs_start;
__private_extern__ kmod_start_func_t *_antimain = procfs_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
