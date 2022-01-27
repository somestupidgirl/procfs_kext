#include <mach/kern_return.h>
#include <mach/kmod.h>
#include <mach/mach_types.h>

#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>

#include <sys/filedesc.h>
#include <sys/kernel_types.h>
#include <sys/mount.h>
#include <sys/types.h>

#include "procfs_vfsops.h"
#include "procfs_vnops.h"

extern struct vfs_fsentry procfs_vfsentry;

kern_return_t procfs_start(__unused kmod_info_t *kinfo, __unused void *data);
kern_return_t procfs_stop(__unused kmod_info_t *kinfo, __unused void *data);

kern_return_t
procfs_start(__unused kmod_info_t *ki, __unused void *d)
{
    int ret;
    struct vfsconf *vfsconf;
    vfstable_t procfs_vfs_table_ref = NULL;

    ret = vfs_fsadd(&procfs_vfsentry, &procfs_vfs_table_ref);
    if (ret != 0) {
        procfs_vfs_table_ref = NULL;
        goto error;
    }

    ret = procfs_init(vfsconf);
    if (ret != KERN_SUCCESS) {
        goto error;
    }

    return KERN_SUCCESS;

error:
    if (procfs_vfs_table_ref) {
        (void)vfs_fsremove(procfs_vfs_table_ref);
    }
    procfs_fini();

    return KERN_FAILURE;
}

kern_return_t
procfs_stop(__unused kmod_info_t *ki, __unused void *d)
{
    int ret;
    vfstable_t procfs_vfs_table_ref = NULL;

    ret = vfs_fsremove(procfs_vfs_table_ref);
    if (ret != KERN_SUCCESS) {
        return KERN_FAILURE;
    }
    procfs_fini();

    return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL ("com.stupid.filesystems.procfs", 1, procfs_start, procfs_stop)
  __attribute__ ((visibility ("default")))

__private_extern__ kmod_start_func_t *_realmain = procfs_start;
__private_extern__ kmod_start_func_t *_antimain = procfs_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
