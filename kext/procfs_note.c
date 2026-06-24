/*	$NetBSD: procfs_note.c,v 1.15 2006/11/16 01:33:38 christos Exp $	*/

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)procfs_note.c	8.2 (Berkeley) 1/21/94
 */

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)procfs_note.c	8.2 (Berkeley) 1/21/94
 */
/*
 * Copyright (c) 2026 Sunneva N. Mariu
 */
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <fs/procfs/procfs.h>

/*
 * Reads a user-supplied note from a write uio into buf (NUL-terminated, with
 * trailing newlines stripped). Mirrors NetBSD's vfs_getuserstr(), which XNU
 * does not provide. *buflenp is the buffer capacity on entry and the note
 * length on return.
 */
static int
procfs_getuserstr(uio_t uio, char *buf, int *buflenp)
{
	size_t xlen = (size_t)uio_resid(uio);

	if (xlen > (size_t)*buflenp) {
		return EMSGSIZE;
	}

	int error = uiomove(buf, (int)xlen, uio);
	if (error) {
		return error;
	}

	buf[xlen] = '\0';
	while (xlen > 0 && buf[xlen - 1] == '\n') {
		buf[--xlen] = '\0';
	}

	*buflenp = (int)xlen;
	return 0;
}

/*
 * /proc/<pid>/note
 *
 * Modeled on NetBSD's procfs_donote(). The node is write-only: a process
 * writes a note that is delivered to the target process. The procfs kext's
 * node model is currently read-only (there is no vnop_write), so the write
 * branch below is not yet reachable and reads return EINVAL, as they do on
 * NetBSD. Delivering the note to the process is not implemented, so the write
 * path reports EOPNOTSUPP.
 */
int
procfs_donote(__unused pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
	char note[PROCFS_NOTELEN + 1];
	int xlen;
	int error;

	if (uio_rw(uio) != UIO_WRITE) {
		return EINVAL;
	}

	xlen = PROCFS_NOTELEN;
	error = procfs_getuserstr(uio, note, &xlen);
	if (error) {
		return error;
	}

	/* send to process's notify function */
	return EOPNOTSUPP;
}
