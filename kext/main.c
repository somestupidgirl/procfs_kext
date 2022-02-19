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

#include <miscfs/procfs/procfs.h>

#include <libbsdkextlog/kauth.h>
#include <libbsdkextlog/kextlog.h>
#include <libbsdkextlog/log_kctl.h>
#include <libbsdkextlog/log_sysctl.h>

#include <libkext/libkext.h>

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
    uuid_string_t uuid;
    struct vfsconf *vfsc;
    kern_return_t ret = KERN_SUCCESS;
    struct mach_header_64 *mh; 

    log_debug("%s \n", version);     /* Print darwin kernel version */

    log_sysctl_register();

    ret = kauth_register();
    if (ret != KERN_SUCCESS) {
        goto out_error;
    }

    ret = log_kctl_register();
    if (ret != KERN_SUCCESS) {
        kauth_deregister();
        goto out_error;
    }

    ret = libkext_vma_uuid(ki->address, uuid);
    kassert(ret == KERN_SUCCESS);
    log_debug("kext executable uuid %s \n", uuid);

    ret = procfs_init(vfsc); // vfsc here??? :s
    if (ret != KERN_SUCCESS) {
        log_error("procfs_init() failed errno:  %d \n", ret);
        goto out_error;
    }
    log_debug("lock group(%s) allocated \n", PROCFS_LCK_GRP_NAME);

    ret = vfs_fsadd(&procfs_vfsentry, &procfs_vfs_table_ref);
    if (ret != KERN_SUCCESS) {
        log_error("vfs_fsadd() failure  errno: %d \n", ret);
        procfs_vfs_table_ref = NULL;
        goto out_vfsadd;
    }
    log_debug("%s file system registered", procfs_vfsentry.vfe_fsname);

    log_debug("loaded %s version %s build %s (%s) \n",
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
    uuid_string_t uuid;
    kern_return_t ret = KERN_SUCCESS;

    ret = libkext_vma_uuid(ki->address, uuid);
    if (ret != KERN_SUCCESS) {
        log_error("util_vma_uuid() failed  errno: %d \n", ret);
        goto out_error;
    }

    ret = log_kctl_deregister();
    if (ret != KERN_SUCCESS) {
        log_error("log_kctl_deregister() failed  errno: %d \n", ret);
        goto out_error;
    }

    ret = vfs_fsremove(procfs_vfs_table_ref);
    if (ret != KERN_SUCCESS) {
        log_error("vfs_fsremove() failure  errno: %d \n", ret);
        goto out_error;
    }

    procfs_fini();
    kauth_deregister();
    log_sysctl_deregister();
    libkext_massert();

    log_debug("unloaded %s version %s build %s (%s) \n",
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
