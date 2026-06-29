/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_taskdump.c
 *
 * TEMPORARY diagnostic node: /proc/<pid>/taskdump dumps the raw bytes of the
 * target's `struct task` (plus a small header), so the matching offsets for the
 * remaining proc_taskinfo fields can be discovered empirically against
 * proc_pidinfo() ground truth (see tools/analysis) instead of guessing the
 * kernel's CONFIG_* flags. Remove once the offsets are baked in.
 *
 * Header layout (little-endian, 32 bytes):
 *   [0] u64 magic = 0x504d554b53415401   ("\x01TASKUMP")
 *   [1] u64 task pointer
 *   [2] u64 proc pointer (p)
 *   [3] u64 number of raw task bytes that follow
 * followed by the raw task bytes.
 */
#include <stdint.h>
#include <string.h>
#include <libkern/libkern.h>
#include <mach/mach_types.h>
#if defined(__x86_64__)
#include <mach/i386/vm_param.h>
#else
#include <mach/vm_param.h>
#endif
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <bsdcompat/sys/malloc.h>

#include <fs/procfs/procfs.h>

extern ppnum_t pmap_find_phys(pmap_t pmap, addr64_t va);
extern pmap_t  kernel_pmap;

#define PROCFS_TASKDUMP_HDR  32
#define PROCFS_TASKDUMP_LEN  6144     /* enough to cover struct task on this build */
#define PROCFS_TASKDUMP_MAGIC 0x504d554b53415401ULL

int
procfs_dotaskdump(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx)
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

    task_t task = proc_task(p);
    if (task == TASK_NULL) {
        proc_rele(p);
        return EIO;
    }

    char *buf = malloc(PROCFS_TASKDUMP_HDR + PROCFS_TASKDUMP_LEN, M_TEMP, M_WAITOK);
    if (buf == NULL) {
        proc_rele(p);
        return ENOMEM;
    }
    bzero(buf, PROCFS_TASKDUMP_HDR + PROCFS_TASKDUMP_LEN);

    uint64_t *hdr = (uint64_t *)buf;
    hdr[0] = PROCFS_TASKDUMP_MAGIC;
    hdr[1] = (uint64_t)(uintptr_t)task;
    hdr[2] = (uint64_t)(uintptr_t)p;

    /* Page-validated copy of the task struct bytes (stop at the first
     * non-resident page so an overrun past the allocation can never fault). */
    size_t copied = 0;
    while (copied < PROCFS_TASKDUMP_LEN) {
        uintptr_t va = (uintptr_t)task + copied;
        if (pmap_find_phys(kernel_pmap, (addr64_t)va) == 0) {
            break;
        }
        size_t chunk = (size_t)(PAGE_SIZE - (va & PAGE_MASK));
        if (chunk > PROCFS_TASKDUMP_LEN - copied) {
            chunk = PROCFS_TASKDUMP_LEN - copied;
        }
        memcpy(buf + PROCFS_TASKDUMP_HDR + copied, (const void *)va, chunk);
        copied += chunk;
    }
    hdr[3] = (uint64_t)copied;

    proc_rele(p);

    error = procfs_copy_data(buf, (int)(PROCFS_TASKDUMP_HDR + copied), uio);
    free(buf, M_TEMP);
    return error;
}
