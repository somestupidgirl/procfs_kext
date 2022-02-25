#include <IOKit/IOLib.h>

#include <kern/clock.h>

#include <mach/kern_return.h>
#include <mach/kmod.h>
#include <mach/mach_types.h>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include <libkern/libkern.h>
#include <libkern/version.h>

#include <sys/kernel_types.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <vm/vm_kern.h>

#include <libkext/libkext.h>
#include <miscfs/procfs/procfs.h>

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
    struct vfsconf *vfsc;
    int ret = KERN_SUCCESS;
    uuid_string_t uuid;

    LOG_DBG("%s \n", version);     /* Print darwin kernel version */

    ret = libkext_vma_uuid(ki->address, uuid);
    kassert(ret == KERN_SUCCESS);
    LOG_DBG("kext executable uuid %s \n", uuid);

    ret = procfs_init(vfsc);
    if (ret != 0) {
        LOG_ERR("procfs_init() failed errno:  %d \n", ret);
        goto out_error;
    }
    LOG_DBG("lock group(%s) allocated \n", PROCFS_LCK_GRP_NAME);

    ret = vfs_fsadd(&procfs_vfsentry, &procfs_vfs_table_ref);
    if (ret != 0) {
        LOG_ERR("vfs_fsadd() failure  errno: %d \n", ret);
        procfs_vfs_table_ref = NULL;
        goto out_vfsadd;
    }
    LOG_DBG("%s file system registered", procfs_vfsentry.vfe_fsname);

    LOG_DBG("loaded %s version %s build %s (%s) \n",
        PROCFS_BUNDLEID, PROCFS_VERSION, PROCFS_BUILDNUM, __TS__);

    return KERN_SUCCESS;

out_vfsadd:
    procfs_fini();

out_error:
    return KERN_FAILURE;
}

kern_return_t
procfs_stop(__unused kmod_info_t *ki, __unused void *d)
{
    uuid_string_t uuid;
    kern_return_t ret = KERN_SUCCESS;

    ret = libkext_vma_uuid(ki->address, uuid);
    if (ret != KERN_SUCCESS) {
        LOG_ERR("util_vma_uuid() failed  errno: %d \n", ret);
        goto out_error;
    }

    ret = vfs_fsremove(procfs_vfs_table_ref);
    if (ret != KERN_SUCCESS) {
        LOG_ERR("vfs_fsremove() failure  errno: %d \n", ret);
        goto out_error;
    }

    procfs_fini();

    libkext_massert();

    LOG_DBG("unloaded %s version %s build %s (%s) \n",
        BUNDLEID_S, KEXTVERSION_S, KEXTBUILD_S, __TS__);

    if (ret == KERN_SUCCESS) {
        goto out_exit;
    }

out_exit:
    return ret;

out_error:
    ret = KERN_FAILURE;
    goto out_exit;
}

KMOD_EXPLICIT_DECL (PROCFS_BUNDLEID, PROCFS_BUILDNUM, procfs_start, procfs_stop)
  __attribute__ ((visibility ("default")))

__private_extern__ kmod_start_func_t *_realmain = procfs_start;
__private_extern__ kmod_start_func_t *_antimain = procfs_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
