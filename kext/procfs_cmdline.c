/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * procfs_cmdline.c
 *
 * Partially based on NetBSD's procfs_cmdline.c
 */
#include <stdint.h>
#include <string.h>
#include <libkern/libkern.h>
#include <libkext/libkext.h>
#include <mach/i386/vm_param.h>
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

#include <miscfs/procfs/procfs.h>

#include "lib/malloc.h"
#include "lib/symbols.h"

int
procfs_docmdline(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;

    /*
     * Find a process.
     */
    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);

    /*
     * Set up variables for the uio move.
     */
    size_t start = 0;
    int len = 0, xlen = 0;
    user_ssize_t resid = uio_resid(uio);
    vm_offset_t off = uio_offset(uio);
    vm_offset_t pgno = trunc_page(off);
    off_t pgoff = (off - pgno);

    /*
     * Allocate a temporary buffer to hold the arguments.
     */
    const char *buf = malloc(PAGE_SIZE, M_TEMP, M_WAITOK);

    if (p != PROC_NULL) {
        /*
         * Don't allow writing.
         */
        if (uio_rw(uio) != UIO_READ) {
            error = EOPNOTSUPP;
        } else {
            /*
             * Zombies don't have a stack, so we can't read their psstrings.
             * System processes also don't have a user stack. This is what
             * ps(1) would display.
             */
            if ((p->p_stat == SZOMB) || (p->p_flag & P_SYSTEM) != 0) {
                char *comm = proc_best_name(p);
                len = snprintf(buf, PAGE_SIZE, "(%s)\n", comm) + 1;
                xlen = (len - pgoff);
                error = uiomove(buf, xlen, uio);
            } else {
                len = snprintf(buf, PAGE_SIZE, "Feature not yet implemented.\n");
                xlen = (len - pgoff);
                error = uiomove(buf, xlen, uio);
            }
        }
        /*
         * Release the process.
         */
        proc_rele(p);
    }
    /*
     * Free the allocated buffer.
     */
    free(buf, M_TEMP);

    return error;
}
