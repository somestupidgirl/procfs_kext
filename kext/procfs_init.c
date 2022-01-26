#include <mach/kern_return.h>
#include <mach/mach_types.h>

#include <kern/locks.h>

#include <libkern/libkern.h>
#include <libkern/locks.h>
#include <libkern/OSMalloc.h>

#include <sys/filedesc.h>
#include <sys/mount.h>
#include <sys/types.h>

#include "procfs.h"
#include "utils.h"

extern vfstable_t procfs_vfs_table_ref;
extern struct vfs_fsentry procfs_vfsentry;

kern_return_t procfs_start (kmod_info_t *kinfo, void *data);
kern_return_t procfs_stop (kmod_info_t *kinfo, void *data);

kern_return_t
procfs_start(__unused kmod_info_t *ki, __unused void *d)
{
    int ret;
    struct vfsconf *vfsconf;

    ret = vfs_fsadd(&procfs_vfsentry, &procfs_vfs_table_ref);
    if (ret != 0) {
        procfs_vfs_table_ref = NULL;
        goto error;
    }

    ret = procfs_init(vfsconf);
    if (ret != KERN_SUCCESS) {
        goto error;
    }

    log("procfs: starting");

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

    ret = vfs_fsremove(procfs_vfs_table_ref);
    if (ret != KERN_SUCCESS) {
        return KERN_FAILURE;
    }
    procfs_fini();

    log("procfs: stopping");

    return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL (PROCFS_BUNDLEID, PROCFS_BUILD, procfs_start, procfs_stop)
  __attribute__ ((visibility ("default")))

__private_extern__ kmod_start_func_t *_realmain = procfs_start;
__private_extern__ kmod_start_func_t *_antimain = procfs_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
