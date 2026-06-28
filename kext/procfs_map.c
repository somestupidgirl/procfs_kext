/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_map.c
 *
 * Process virtual-memory map enumeration and the NetBSD-format node:
 *   /proc/<pid>/map   - NetBSD procfs format (procfs_domap, here)
 *   /proc/<pid>/maps  - Linux format (procfs_domaps, in procfs_linux.c)
 * References:
 *   https://github.com/NetBSD/src/blob/trunk/sys/miscfs/procfs/procfs_map.c
 *   Linux Documentation/filesystems/proc.rst (/proc/<pid>/maps)
 *
 * Both nodes share the region walk in procfs_map_render() and differ only in
 * the per-region formatter. The walk uses mach_vm_region(), resolved from the
 * on-disk kernel collection via libklookup along with get_task_map(): macOS
 * exports no region-enumeration KPI a third-party kext may link, and the
 * internal walkers (vm_map_region/vm_map_lookup_entry) are stripped from the
 * arm64 kernel. mach_vm_region() takes the map's read lock internally, so this
 * needs no struct-walking or manual locking. For VM_REGION_BASIC_INFO_64 the
 * call sets object_name to IP_NULL, so there is no port reference to release.
 *
 * Backing-file paths (the trailing column on Linux) are not resolved: that
 * needs the region's memory object -> vnode, which is not reachable here.
 */
#include <stdint.h>
#include <string.h>
#include <libkern/libkern.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/vm_types.h>
#include <mach/vm_prot.h>
#include <mach/vm_region.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/sbuf.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <ptrauth.h>

#include <bsdcompat/sys/malloc.h>

#include <fs/procfs/procfs.h>

#include "lib/symbols.h"

/*
 * Resolved-symbol function-pointer types. The mach_vm_region() prototype uses
 * the "user typed" (_ut) wrappers, but those are transparent_unions that are
 * ABI-identical to the plain mach_vm_offset_t/mach_vm_size_t, so a plain-typed
 * pointer is calling-convention correct. object_name is taken as void * so the
 * kernel's 8-byte IP_NULL write lands in a pointer-sized slot regardless of how
 * mach_port_t is typedef'd here.
 */
typedef vm_map_t (*procfs_get_task_map_fn)(task_t task);
typedef kern_return_t (*procfs_mach_vm_region_fn)(vm_map_t map,
    mach_vm_offset_t *address, mach_vm_size_t *size, int flavor,
    int *info, mach_msg_type_number_t *count, void *object_name);

/* Guard against a pathological walk (mach_vm_region terminates with a non-
 * success return at the top of the address space anyway). */
#define PROCFS_MAP_MAX_REGIONS 1000000

/*
 * Shared region walk for the map/maps nodes. Enumerates the process's VM
 * regions and invokes `fmt` once per region to append a formatted line.
 */
int
procfs_map_render(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx, procfs_region_fmt_fn fmt)
{
    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p == PROC_NULL) {
        return ESRCH;
    }

    int error = procfs_check_can_access_process(vfs_context_ucred(ctx), p);
    if (error != 0) {
        proc_rele(p);
        return error;
    }

    if (procfs_kl_get_task_map == NULL || procfs_kl_mach_vm_region == NULL) {
        proc_rele(p);
        return ENOTSUP;     /* libklookup did not resolve the VM symbols */
    }

    /* Sign the resolved addresses for the arm64e ABI (disc 0; the assignment
     * resigns to each call's type discriminator). */
    procfs_get_task_map_fn   get_task_map   =
        ptrauth_sign_unauthenticated(procfs_kl_get_task_map, ptrauth_key_function_pointer, 0);
    procfs_mach_vm_region_fn mach_vm_region =
        ptrauth_sign_unauthenticated(procfs_kl_mach_vm_region, ptrauth_key_function_pointer, 0);

    task_t   task = proc_task(p);
    vm_map_t map  = (task != TASK_NULL) ? get_task_map(task) : NULL;
    if (map == NULL) {
        proc_rele(p);
        return EIO;
    }

    /* A large address space can have thousands of regions, so grow on demand. */
    struct sbuf sb;
    if (sbuf_new(&sb, NULL, 4096, SBUF_AUTOEXTEND) == NULL) {
        proc_rele(p);
        return ENOMEM;
    }

    mach_vm_offset_t addr = 0;
    for (int n = 0; n < PROCFS_MAP_MAX_REGIONS; n++) {
        mach_vm_size_t                 size  = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t         count = VM_REGION_BASIC_INFO_COUNT_64;
        void                          *object_name = NULL;  /* 8-byte slot; gets IP_NULL */

        kern_return_t kr = mach_vm_region(map, &addr, &size, VM_REGION_BASIC_INFO_64,
            (int *)&info, &count, &object_name);
        if (kr != KERN_SUCCESS) {
            break;          /* no more regions at or above addr */
        }

        struct procfs_region r = {
            .start    = (uint64_t)addr,
            .end      = (uint64_t)(addr + size),
            .offset   = (uint64_t)info.offset,
            .prot     = info.protection,
            .max_prot = info.max_protection,
            .shared   = info.shared,
            .wired    = info.user_wired_count,
        };
        fmt(&sb, &r);

        mach_vm_offset_t next = addr + size;
        if (next <= addr) {
            break;          /* wrapped at the top of the address space */
        }
        addr = next;
    }

    proc_rele(p);

    sbuf_finish(&sb);
    error = procfs_copy_data(sbuf_data(&sb), sbuf_len(&sb), uio);
    sbuf_delete(&sb);

    return error;
}

/*
 * NetBSD-style formatter: start-end curprot maxprot sharing wired.
 */
static void
procfs_map_fmt_netbsd(struct sbuf *sb, const struct procfs_region *r)
{
    sbuf_printf(sb, "%#018llx %#018llx %c%c%c %c%c%c %s %u\n",
        (unsigned long long)r->start, (unsigned long long)r->end,
        (r->prot & VM_PROT_READ)        ? 'r' : '-',
        (r->prot & VM_PROT_WRITE)       ? 'w' : '-',
        (r->prot & VM_PROT_EXECUTE)     ? 'x' : '-',
        (r->max_prot & VM_PROT_READ)    ? 'r' : '-',
        (r->max_prot & VM_PROT_WRITE)   ? 'w' : '-',
        (r->max_prot & VM_PROT_EXECUTE) ? 'x' : '-',
        r->shared ? "share" : "priv", r->wired);
}

/*
 * "map" node - NetBSD procfs format.
 */
int
procfs_domap(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx)
{
    return procfs_map_render(pnp, uio, ctx, procfs_map_fmt_netbsd);
}
