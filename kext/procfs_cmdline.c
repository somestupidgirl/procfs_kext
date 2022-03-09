/*	$NetBSD: procfs_cmdline.c,v 1.32 2019/09/27 14:36:18 christos Exp $	*/

/*
 * Copyright (c) 1999 Jaromir Dolecek <dolecek@ics.muni.cz>
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
//__KERNEL_RCSID(0, "$NetBSD: procfs_cmdline.c,v 1.32 2019/09/27 14:36:18 christos Exp $");

#include <stdint.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <miscfs/procfs/procfs.h>
#include <miscfs/procfs/procfs_data.h>

#include "procfs_info.h"
#include "procfs_internal.h"

/*
 * code for returning process's command line arguments/environment
 */
int
procfs_read_cmdline_data(procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
	int error = 0;
	size_t len, start;

	int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);

	/* Don't allow writing. */
	if (uio_rw(uio) != UIO_READ) {
		return EOPNOTSUPP;
	}

	/*
	 * Zombies don't have a stack, so we can't read their psstrings.
	 * System processes also don't have a user stack.  This is what
	 * ps(1) would display.
	 */
	if (p != NULL) {
		_proc_list_lock();
		if (p->p_stat == SZOMB || (p->p_flag & P_SYSTEM) != 0) {
			char msg[] = "()";

			if (uio_offset(uio) == 0) {
				error = procfs_copy_data((char *)&msg, sizeof(1), uio);
				if (error == 0) {
					return error;
				}
			}

			len = strlen(p->p_comm);

			if (len >= sizeof(uio_offset(uio))) {
				start = sizeof(uio_offset(uio)) - 1;
				size_t comm = strlen(p->p_comm + start);
				error = procfs_copy_data((char *)&comm, sizeof(len - start), uio);
				if (error == 0) {
					return error;
				}
			}

			if (len + 2 >= sizeof(uio_offset(uio))) {
				start = (sizeof(uio_offset(uio)) - 1 - len);
				size_t msg_buf = strlen(msg + 1 + start);
				error = procfs_copy_data((char *)&msg_buf, sizeof(2 - start), uio);
			}
		}
	}
	_proc_list_unlock();
	proc_rele(p);
    if (p != NULL) {
        p = NULL;
    }

	return error;
}
