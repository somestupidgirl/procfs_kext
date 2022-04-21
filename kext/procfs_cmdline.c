/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * procfs_cmdline.c
 *
 * Loosely based on NetBSD's procfs_cmdline.c
 */
#include <stdint.h>
#include <string.h>
#include <libkern/libkern.h>
#include <libkext/libkext.h>
#include <mach/i386/vm_param.h>
#include <os/log.h>
#include <sys/errno.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/uio_internal.h>
#include <sys/vm.h>
#include <sys/vnode.h>

#include <miscfs/procfs/procfs.h>

#include "lib/helpers.h"
#include "lib/symbols.h"

int
procfs_docmdline(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;

    /*
     * Get associated proces and current process.
     */
    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
    proc_t curp = current_proc();

    struct exec_info pss;
    struct uio auio;
    struct iovec aiov;
    struct vmspace *vm;
    const char *buf;

    /*
     * Set up variables for the uio move.
     */
    int len = 0, xlen = 0;
    vm_offset_t off = uio_offset(uio);
    vm_offset_t pgno = trunc_page(off);
    off_t pgoff = (off - pgno);

    if (p != PROC_NULL) {
        /*
         * Don't allow writing.
         */
        if (uio_rw(uio) != UIO_READ) {
            error = EOPNOTSUPP;
        } else {
            /*
             * Allocate a temporary buffer to hold the arguments.
             */
            buf = malloc(PAGE_SIZE, M_TEMP, M_WAITOK);

            /*
             * Zombies don't have a stack, so we can't read their psstrings.
             * System processes also don't have a user stack. This is what
             * ps(1) would display.
             */
            if (p->p_stat != SZOMB || (p->p_flag & P_SYSTEM) != 0) {
                len = snprintf(buf, PAGE_SIZE, "(%s)\n", p->p_comm) + 1;
            } else {
                /*
                 * Read in the exec_info structure.
                 */
                // TODO

                /*
                 * Now read the address of the argument vector.
                 */
                // TODO

                /*
                 * Now copy in the actual argument vector, one page at a time,
                 * since we don't know how long the vector is (though, we do
                 * know how many NUL-terminated strings are in the vector).
                 */
                // TODO

                len = snprintf(buf, PAGE_SIZE, "Error: Feature not yet implemented.\n");
            }
            /*
             * Do the uio move.
             */
            xlen = (len - pgoff);
            error = uiomove(buf, xlen, uio);

            /*
             * Free the allocated buffer.
             */
            free(buf, M_TEMP);
        }
        /*
         * Release the process.
         */
        proc_rele(p);
    }

    return error;
}
