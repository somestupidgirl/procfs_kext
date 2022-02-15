#include <kern/clock.h>

#include <mach/kern_return.h>
#include <mach/kmod.h>
#include <mach/mach_types.h>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include <os/log.h>

#include <libkern/libkern.h>
#include <libkern/version.h>

#include <sys/kernel_types.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <vm/vm_kern.h>

#include <miscfs/procfs/procfs.h>

#include "libksym/ksym.h"
#include "libksym/utils.h"

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
    vm_offset_t vm_kern_ap_ext;
    vm_offset_t vm_kern_slide;
    vm_address_t hib_base;
    vm_address_t kern_base;
    struct mach_header_64 *mh;
    struct vfsconf *vfsc;
    kern_return_t ret = KERN_SUCCESS;

    LOG("%s \n", version);     /* Print darwin kernel version */

    ret = util_vma_uuid(ki->address, uuid);
    kassert(ret == KERN_SUCCESS);
    LOG("kext executable uuid %s \n", uuid);

    vm_kern_ap_ext = get_vm_kernel_addrperm_ext();

    if (vm_kern_ap_ext == 0) {
        panic("get_vm_kernel_addrperm_ext() failed \n");
        goto out_error;
    }
    LOG("vm_kernel_addrperm_ext: %#018lx \n", vm_kern_ap_ext);

    vm_kern_slide = get_vm_kernel_slide();

    if (vm_kern_slide == 0) {
        panic("get_vm_kernel_slide() failed");
        goto out_error;
    }
    LOG("vm_kernel_slide:        %#018lx \n", vm_kern_slide);

    hib_base = KERN_HIB_BASE + vm_kern_slide;
    kern_base = KERN_TEXT_BASE + vm_kern_slide;

    LOG("HIB text base:          %#018lx \n", hib_base);
    LOG("kernel text base:       %#018lx \n", kern_base);

    mh = (struct mach_header_64 *) kern_base;
    if ((mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) || (mh->filetype != MH_EXECUTE && mh->filetype != MH_FILESET))
    {
        panic("bad mach header  mh: %p mag: %#010x type: %#010x", mh, mh->magic, mh->filetype);
        goto out_error;
    }

    LOG("magic:                  %#010x \n", mh->magic);
    LOG("cputype:                %#010x \n", mh->cputype);
    LOG("cpusubtype:             %#010x \n", mh->cpusubtype);
    LOG("filetype:               %#010x \n", mh->filetype);
    LOG("ncmds:                  %#010x \n", mh->ncmds);
    LOG("sizeofcmds:             %#010x \n", mh->sizeofcmds);
    LOG("flags:                  %#010x \n", mh->flags);
    LOG("reserved:               %#010x \n", mh->reserved);

    ret = procfs_init(vfsc);
    if (ret != KERN_SUCCESS) {
        LOG_ERR("procfs_init() fail");
        goto out_error;
    }
    LOG("lock group(%s) allocated \n", PROCFS_LCK_GRP_NAME);

    ret = vfs_fsadd(&procfs_vfsentry, &procfs_vfs_table_ref);
    if (ret != KERN_SUCCESS) {
        LOG_ERR("vfs_fsadd() failure  errno: %d \n", ret);
        procfs_vfs_table_ref = NULL;
        goto out_vfsadd;
    }
    LOG("%s file system registered", procfs_vfsentry.vfe_fsname);

    LOG("loaded %s version %s build %s (%s) \n",
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

    ret = util_vma_uuid(ki->address, uuid);
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

    util_massert();

    LOG("unloaded %s version %s build %s (%s) \n",
        BUNDLEID_S, KEXTVERSION_S, KEXTBUILD_S, __TS__);

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
