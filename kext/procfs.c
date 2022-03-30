#include <kern/locks.h>
#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>
#include <libkern/version.h>
#include <libkext/libkext.h>
#include <mach/kmod.h>
#include <mach/mach_types.h>
#include <sys/mount.h>

#include <miscfs/procfs/procfs.h>

#include "symbols.h"

#pragma mark -
#pragma mark Start/Stop Function Prototypes

kern_return_t procfs_start(kmod_info_t *ki, void *d);
kern_return_t procfs_stop(kmod_info_t *ki, void *d);

#pragma mark -
#pragma mark External References

extern struct vfs_fsentry procfs_vfsentry;
extern vfstable_t procfs_vfs_table_ref;

#pragma mark -
#pragma mark Initialization and finishing routines

/*
 * Initialization. Initializes static data, which is required when
 * the first mount occurs. Called only once during kernel startup,
 * but we interlock anyway to ensure that we don't perform intialization
 * more than once.
 */
int
procfs_init(__unused struct vfsconf *vfsconf)
{
    static int initialized;  // Protect against multiple calls.

    if (!initialized) {
        initialized = 1;

        // Create the tag for memory allocation.
        procfs_osmalloc_tag = OSMalloc_Tagalloc(PROCFS_BUNDLEID, OSMT_DEFAULT);

        if (procfs_osmalloc_tag == NULL) {
            return ENOMEM;   // Plausible error code.
        }

        // Allocate the lock group and the mutex lock for the hash table.
        pfsnode_lck_grp = lck_grp_alloc_init(PROCFS_BUNDLEID ".pfsnode_locks", LCK_GRP_ATTR_NULL);
        pfsnode_hash_mutex = lck_mtx_alloc_init(pfsnode_lck_grp, LCK_ATTR_NULL);
    }

    return 0;
}

/*
 * Cleanup routine. Free the memory allocation tag, lock group and
 * hash mutex upon unloading the kext.
 */
int
procfs_fini(void)
{
    if (procfs_osmalloc_tag != NULL) {
        OSMalloc_Tagfree(procfs_osmalloc_tag);
        procfs_osmalloc_tag = NULL;
    }

    if (pfsnode_lck_grp != NULL) {
        lck_grp_free(pfsnode_lck_grp);
        pfsnode_lck_grp = NULL;
    }

    if (pfsnode_hash_mutex != NULL) {
        lck_mtx_free(pfsnode_hash_mutex, pfsnode_lck_grp);
        pfsnode_hash_mutex = NULL;
    }

    return 0;
}

#pragma mark -
#pragma mark Start/Stop Routines

kern_return_t
procfs_start(kmod_info_t *ki, __unused void *d)
{
    struct vfsconf *vfsc = NULL;
    int ret = 0;
    uuid_string_t uuid;

    LOG_DBG("%s \n", version);     /* Print darwin kernel version */

    ret = libkext_vma_uuid(ki->address, uuid);
    kassert(ret == 0);
    LOG_DBG("kext executable uuid %s \n", uuid);

    ret = resolve_symbols();
    if (ret != KERN_SUCCESS) {
        LOG_ERR("resolve_symbols() failed errno:  %d \n", ret);
        goto out_error;
    }

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
    kern_return_t ret = 0;

    ret = libkext_vma_uuid(ki->address, uuid);
    if (ret != 0) {
        LOG_ERR("util_vma_uuid() failed  errno: %d \n", ret);
        goto out_error;
    }

    ret = vfs_fsremove(procfs_vfs_table_ref);
    if (ret != 0) {
        LOG_ERR("vfs_fsremove() failure  errno: %d \n", ret);
        goto out_error;
    }

    procfs_fini();
    libkext_massert();

    LOG_DBG("unloaded %s version %s build %s (%s) \n",
        BUNDLEID_S, KEXTVERSION_S, KEXTBUILD_S, __TS__);

    return KERN_SUCCESS;

out_error:
    return KERN_FAILURE;
}

KMOD_EXPLICIT_DECL (PROCFS_BUNDLEID, PROCFS_BUILDNUM, procfs_start, procfs_stop)
  __attribute__ ((visibility ("default")))

__private_extern__ kmod_start_func_t *_realmain = procfs_start;
__private_extern__ kmod_start_func_t *_antimain = procfs_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
