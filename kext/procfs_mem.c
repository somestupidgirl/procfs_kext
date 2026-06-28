/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_mem.c
 *
 * Process virtual-memory node (/proc/<pid>/mem). The entry point is named after
 * NetBSD's procfs_mem.c (procfs_domem); FreeBSD's equivalent is procfs_doprocmem
 * in the same file. References:
 *   https://github.com/NetBSD/src/blob/trunk/sys/miscfs/procfs/procfs_mem.c
 *   https://github.com/freebsd/freebsd-src/blob/main/sys/fs/procfs/procfs_mem.c
 *
 * As with the Linux /proc/<pid>/mem file, the read offset is interpreted as a
 * virtual address in the target process: seeking to an address and reading
 * returns the bytes at that address. The overall shape matches both BSDs - a
 * credential/visibility check (NetBSD process_checkioperm(), FreeBSD
 * p_candebug()) followed by a memory-read loop driven by the uio.
 *
 * The read mechanism itself diverges. Both BSDs fault the target's pages in
 * through the VM system (NetBSD uvm_io(), FreeBSD proc_rwmem()); macOS exposes
 * no equivalent KPI a third-party kext may link (the vm_map_copyin() path is
 * com.apple.kpi.private), so we instead translate the target's user pages to
 * physical frames through its pmap and read them via the physical aperture - all
 * linkable (com.apple.kpi.unsupported): get_task_pmap(), pmap_find_phys() and
 * ml_phys_read(). This is the same approach the cmdline node uses (see
 * procfs_cmdline.c). One consequence of not faulting is that only resident
 * pages are returned; a paged-out address ends the read rather than being
 * paged back in. Because the implementation differs substantially from the BSD
 * originals, their source license headers are not reproduced here.
 */
#include <stdint.h>
#include <string.h>
#include <libkern/libkern.h>
#if defined(__x86_64__)
#include <mach/i386/vm_param.h>
#elif defined(__arm64__) || defined(__aarch64__)
#include <mach/vm_param.h>
#endif
#include <mach/vm_types.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <bsdcompat/sys/malloc.h>

#include <fs/procfs/procfs.h>

/*
 * Physical-aperture read primitives (com.apple.kpi.unsupported, linkable). See
 * the file banner and procfs_cmdline.c for why these are used in place of the
 * private vm_map_copyin() path.
 */
extern pmap_t       get_task_pmap(task_t task);
extern ppnum_t      pmap_find_phys(pmap_t pmap, addr64_t va);
extern unsigned int ml_phys_read(vm_offset_t paddr);

/*
 * Best-effort copy of `len` bytes from user virtual address `uva` in the address
 * space described by `pmap` into kernel buffer `dst`. Returns the number of
 * bytes copied, stopping at the first page that is not resident. Reads go
 * through the physical aperture, so a paged-out page is simply not returned
 * rather than faulted in.
 */
static size_t
procfs_read_user_phys(pmap_t pmap, user_addr_t uva, uint8_t *dst, size_t len)
{
    size_t done = 0;

    while (done < len) {
        user_addr_t va = uva + done;
        ppnum_t ppn = pmap_find_phys(pmap, (addr64_t)va);
        if (ppn == 0) {
            break;                              /* not resident - stop here */
        }

        addr64_t pa = (addr64_t)ptoa_64(ppn) + (va & PAGE_MASK);
        size_t   chunk = (size_t)(PAGE_SIZE - (va & PAGE_MASK));
        if (chunk > len - done) {
            chunk = len - done;
        }

        for (size_t i = 0; i < chunk;) {
            addr64_t cur = pa + i;
            uint32_t word = ml_phys_read((vm_offset_t)(cur & ~3ULL));
            unsigned shift = (unsigned)(cur & 3) * 8;   /* little-endian byte */
            do {
                dst[done + i] = (uint8_t)(word >> shift);
                shift += 8;
                i++;
            } while (shift < 32 && i < chunk);
        }
        done += chunk;
    }

    return done;
}

/*
 * Reads the data for the "mem" node: bytes from the target process's address
 * space, with the uio offset interpreted as the virtual address (the Linux
 * /proc/<pid>/mem semantics). Named after NetBSD's procfs_domem().
 *
 * Returns EIO if the very first requested address is not resident (an unmapped
 * region or a paged-out address), and stops cleanly at the first hole once some
 * bytes have been returned - matching how reads of /proc/<pid>/mem behave across
 * unmapped gaps.
 */
int
procfs_domem(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx)
{
    int error = 0;

    /* The node is read-only; writing another process's memory is not supported. */
    if (uio_rw(uio) != UIO_READ) {
        return EOPNOTSUPP;
    }

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p == PROC_NULL) {
        return ESRCH;
    }

    /* Reading another process's memory is sensitive; gate on the same
     * credential check the rest of the filesystem uses. */
    error = procfs_check_can_access_process(vfs_context_ucred(ctx), p);
    if (error != 0) {
        proc_rele(p);
        return error;
    }

    /*
     * Hold the proc_find() reference across the whole read: that keeps
     * proc->task referenced so the task's map/pmap stay valid. If the process
     * exits mid-read its pages simply stop being resident and the read ends at
     * that point (see the cmdline node for the same reasoning).
     */
    task_t task = proc_task(p);
    pmap_t pmap = (task != TASK_NULL) ? get_task_pmap(task) : NULL;
    if (pmap == NULL) {
        proc_rele(p);
        return EIO;
    }

    uint8_t *buf = malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
    if (buf == NULL) {
        proc_rele(p);
        return ENOMEM;
    }

    boolean_t any = FALSE;
    while (uio_resid(uio) > 0) {
        user_addr_t  va    = (user_addr_t)uio_offset(uio);
        size_t       pgoff = (size_t)(va & PAGE_MASK);
        size_t       chunk = (size_t)PAGE_SIZE - pgoff;     /* up to a page boundary */
        user_ssize_t resid = uio_resid(uio);
        if ((user_ssize_t)chunk > resid) {
            chunk = (size_t)resid;
        }

        size_t got = procfs_read_user_phys(pmap, va, buf, chunk);
        if (got == 0) {
            if (!any) {
                error = EIO;        /* nothing readable at the start address */
            }
            break;                  /* otherwise return what we have */
        }

        error = uiomove((const char *)buf, (int)got, uio);
        if (error != 0) {
            break;
        }
        any = TRUE;

        if (got < chunk) {
            break;                  /* hit a non-resident page - stop at the hole */
        }
    }

    free(buf, M_TEMP);
    proc_rele(p);
    return error;
}
