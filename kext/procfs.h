//
//  procfs.h
//  ProcFS
//
//  Created by Kim Topley on 12/1/15.
//
// Definitions for the procfs file system. The content of this
// header is available to both user-level and kernel components.
//

#ifndef procfs_h
#define procfs_h

#include "procfs_internal.h"

#pragma mark -
#pragma mark Kext Initialization Definitions

#define PROCFS_NAME         "procfs"
#define PROCFS_FSTYPENUM    0
#define PROCFS_LCKGRP_NAME  PROCFS_NAME ".lock"

#define VFC_VFS64BITREADY   0x100
#define VFC_VFSNOMACLABEL   0x1000

#define PROCFS_VFS_FLAGS    (VFS_TBL64BITREADY | VFC_VFSNOMACLABEL)

extern struct vnodeopv_desc *procfs_vnopv_desc_list[PROCFS_FSTYPENUM];

#pragma mark -
#pragma mark Common Definitions

// File system type name.
#define PROCFS_FSNAME "procfs"

// Mount option flags.
// Do not apply process permissions to the pid entries in /proc.
#define PROCFS_MOPT_NOPROCPERMS (1 << 0)

/*
 * The procfs mount structure, created by mount_procfs
 * and passed to the kernel by the mount(2) system call.
 */
typedef struct procfs_mount_args {
    int mnt_options;      // The procfs mount options.
} procfs_mount_args_t;

extern struct vfsops procfs_vfsops;

#pragma mark -
#pragma mark Internel Definitions - Kernel Only

/* -- Internal definitions. -- */

#include <libkern/OSMalloc.h>
#include "procfs_internal.h"

/* -- Global functions and data -- */
// Tag used for memory allocation.
extern OSMallocTag procfs_osmalloc_tag;

/*
 * Procfs per-moount data structure. Typically, there is only one
 * instance of this file system, but the implementation does not
 * preclude multiple mounts.
 */
typedef struct procfs_mount {
    int32_t         pmnt_id;            // A unique identifier for this mount. Shared by all nodes.
    int             pmnt_flags;         // Flags, set from the mount command (PROCFS_MOPT_XXX).
    struct mount    *pmnt_mp;           // VFS-level mount structure.
    struct timespec pmnt_mount_time;    // Time at which the file system was mounted.
} procfs_mount_t;

// Convert from procfs mount pointer to VFS mount structure
static inline struct mount *procfs_mp_to_vfs_mp(procfs_mount_t *pmp) {
    return pmp->pmnt_mp;
}

// Convert from VFS mount pointer to procfs mount pointer.
static inline procfs_mount_t *vfs_mp_to_procfs_mp(struct mount *vmp) {
    return(procfs_mount_t *)vfs_fsprivate(vmp);
}

// Returns whether access checks should apply to the vnodes on a given mount point.
static inline boolean_t procfs_should_access_check(procfs_mount_t *pmp) {
    return (pmp->pmnt_flags & PROCFS_MOPT_NOPROCPERMS) == 0;
}

#endif /* procfs_h */
