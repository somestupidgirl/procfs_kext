/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_cmdline.c
 *
 * Process argument vector for the "cmdline" node. The entry points are named
 * after NetBSD's procfs_cmdline.c (procfs_doprocargs / procfs_doprocargs_helper)
 * for easy cross-reference:
 *   https://github.com/NetBSD/src/blob/trunk/sys/miscfs/procfs/procfs_cmdline.c
 *
 * The argument-fetching mechanism necessarily diverges. NetBSD's helper is an
 * output callback for the kernel's copy_procargs(); macOS has no equivalent
 * callback KPI a kext may link (KERN_PROCARGS2's vm_map_copyin() path is
 * com.apple.kpi.private), so procfs_doprocargs_helper() here reads the target's
 * user stack directly through its pmap. Because the implementation differs
 * substantially from NetBSD's, that file's source license header is not
 * reproduced here.
 */
#include <stdint.h>
#include <string.h>
#include <libkern/libkern.h>
#include <libkext/libkext.h>
#if defined(__x86_64__)
#include <mach/i386/vm_param.h>
#elif defined(__arm64__) || defined(__aarch64__)
#include <mach/vm_param.h>
#endif
#include <mach/vm_map.h>
#include <mach/vm_types.h>
#include <os/log.h>
#include <sys/errno.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/uio_internal.h>
#include <sys/vm.h>
#include <sys/vnode.h>

#include <bsdcompat/sys/malloc.h>

#include <fs/procfs/procfs.h>

#include "lib/symbols.h"

/*
 * The process arguments live at the top of the target process's user stack, in
 * the region [user_stack - p_argslen, user_stack). KERN_PROCARGS2 reads them
 * with vm_map_copyin(), but that primitive (and vm_map_copy_overwrite, etc.) is
 * com.apple.kpi.private and cannot be linked by a third-party kext. Instead we
 * translate the target's user pages to physical frames through its pmap and read
 * them via the physical aperture - all linkable (com.apple.kpi.unsupported):
 * get_task_pmap(), pmap_find_phys() and ml_phys_read().
 */
extern pmap_t       get_task_pmap(task_t task);
extern ppnum_t      pmap_find_phys(pmap_t pmap, addr64_t va);
extern unsigned int ml_phys_read(vm_offset_t paddr);

/* The bare executable path in the args section is prefixed with this key (see
 * sysctl_procargsx() in XNU's kern_sysctl.c); we skip past it to reach argv. */
#define PROCFS_EXEC_KEY     "executable_path="

/* Upper bound on how much of the args region we read. argv sits at the start of
 * the region (before the environment), so this comfortably covers any real
 * command line while bounding the work for a pathological one. */
#define PROCFS_CMDLINE_MAX  (256 * 1024)

/*
 * Best-effort copy of `len` bytes from user virtual address `uva` in the address
 * space described by `pmap` into kernel buffer `dst`. Returns the number of
 * bytes copied, stopping at the first page that is not resident. Reads go
 * through the physical aperture, so a paged-out page is simply not returned
 * rather than faulted in (the args are normally resident).
 */
size_t
procfs_copy_user_phys(pmap_t pmap, user_addr_t uva, uint8_t *dst, size_t len)
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
 * Emit the parenthesised command name, e.g. "(launchd)", the way ps(1) renders a
 * process with no readable arguments (zombies, system processes, kernel
 * threads). Used as a fallback when the real argument vector is unavailable.
 */
static int
procfs_cmdline_comm(proc_t p, uio_t uio)
{
    char comm[MAXCOMLEN + 1];
    char namebuf[MAXCOMLEN + 4];

    comm[0] = '\0';
    proc_name(proc_pid(p), comm, sizeof(comm));     /* public KPI, offset-independent */

    int len = snprintf(namebuf, sizeof(namebuf), "(%s)", comm[0] ? comm : "unknown");
    return procfs_copy_data(namebuf, len, uio);
}

/*
 * Fetch and emit the argument vector for a live, non-system process. Reads the
 * args region out of the target's user stack via its pmap, parses out argv and
 * copies it to the uio. Mirrors the role of NetBSD's procfs_doprocargs_helper()
 * (which is instead an output callback for copy_procargs(); see the file
 * banner). The caller holds the proc_find() reference.
 */
static int
procfs_doprocargs_helper(proc_t p, uio_t uio)
{
    int error = 0;

    /*
     * Snapshot the argument metadata and the task pmap. The caller's
     * proc_find() reference is held across the whole read: that keeps
     * proc->task referenced, so the task is not deallocated and its map/pmap
     * structs stay valid (the map is torn down at task_deallocate, not at
     * terminate). If the process exits during the read, its pages simply stop
     * being resident and pmap_find_phys() returns 0. (A concurrent execve()
     * could still swap the map out from under us; that sub-microsecond window
     * is unprotectable without vm_map_reference(), which is not linkable, and
     * matches the "stale info acceptable" stance of XNU's own KERN_PROCARGS2.)
     */
    int         argc       = p->p_argc;
    size_t      argslen    = p->p_argslen;
    user_addr_t user_stack = p->user_stack;
    task_t      task       = proc_task(p);
    pmap_t      pmap       = (task != TASK_NULL) ? get_task_pmap(task) : NULL;

    if (argc <= 0 || user_stack == 0 || pmap == NULL ||
        argslen <= sizeof(PROCFS_EXEC_KEY) - 1) {
        return procfs_cmdline_comm(p, uio);
    }

    /*
     * The args region is [user_stack - argslen, user_stack); the leading
     * PROCFS_EXEC_KEY prefix is stripped exactly as sysctl_procargsx() does, so
     * the data starts with the bare executable path followed by argv and then
     * the environment. Read from the start of that data forward.
     */
    size_t      adj     = argslen - (sizeof(PROCFS_EXEC_KEY) - 1);
    user_addr_t data_va = user_stack - adj;
    size_t      readlen = (adj > PROCFS_CMDLINE_MAX) ? PROCFS_CMDLINE_MAX : adj;

    uint8_t *buf = malloc(readlen, M_TEMP, M_WAITOK);
    if (buf == NULL) {
        return ENOMEM;
    }

    size_t n = procfs_copy_user_phys(pmap, data_va, buf, readlen);
    if (n == 0) {
        /* Could not read any pages; fall back to the command name. */
        error = procfs_cmdline_comm(p, uio);
    } else {
        /*
         * Parse the strings: index 0 is the bare executable path (which may be
         * followed by NUL padding for alignment); indices 1..argc are argv. We
         * output just the argv span, which is already NUL-separated/terminated.
         */
        size_t pos = strnlen((const char *)buf, n);
        if (pos < n) {
            pos++;                                       /* path's NUL */
            while (pos < n && buf[pos] == '\0') {
                pos++;                                   /* alignment padding */
            }
        }

        size_t argv_start = pos;
        for (int got = 0; got < argc && pos < n; got++) {
            size_t remaining = n - pos;
            size_t arglen = strnlen((const char *)buf + pos, remaining);
            if (arglen < remaining) {
                arglen++;                                /* include the NUL */
            }
            pos += arglen;
        }

        error = procfs_copy_data((const char *)(buf + argv_start), (int)(pos - argv_start), uio);
    }

    free(buf, M_TEMP);
    return error;
}

/*
 * Reads the data for the "cmdline" node: the process's argument vector, with
 * each argument NUL-terminated (the Linux /proc/<pid>/cmdline format). Named
 * after NetBSD's procfs_doprocargs(); it dispatches the special zombie/system
 * cases here and delegates a live process to procfs_doprocargs_helper().
 */
int
procfs_doprocargs(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p == PROC_NULL) {
        return ESRCH;
    }

    /* The node is read-only. */
    if (uio_rw(uio) != UIO_READ) {
        proc_rele(p);
        return EOPNOTSUPP;
    }

    /*
     * Zombies and system processes have no user stack to read arguments from,
     * so report the command name in parentheses, as ps(1) does.
     */
    if ((p->p_stat == SZOMB) || (p->p_flag & P_SYSTEM) != 0) {
        error = procfs_cmdline_comm(p, uio);
    } else {
        error = procfs_doprocargs_helper(p, uio);
    }

    proc_rele(p);
    return error;
}
