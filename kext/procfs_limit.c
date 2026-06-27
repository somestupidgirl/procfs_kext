/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 1999 Adrian Chadd
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
 */
/*
 * To get resource.h to include our rlimit_ident[] array of rlimit identifiers
 */
/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_limit.c
 * 
 * Based on FreeBSD's procfs_rlimit.c
 * 
 * Process resource-limit node (/proc/<pid>/limit).
 *
 */

#ifdef _RLIMIT_IDENT

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/resourcevar.h>
#include <sys/resource.h>
#include <sys/sbuf.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include <fs/procfs/procfs.h>

/*
 * Resource limit string identifiers
 */
static const char *rlimit_ident[] = {
	"cpu",
	"fsize",
	"data",
	"stack",
	"core",
	"rss",
	"memlock",
	"nproc",
	"nofile",
	"sbsize",
	"vmem",
	"npts",
	"swap",
	"kqueues",
	"umtx",
	"pipebuf",
	"vms",
};

/*
 * proc_limitblock/unblock are used to serialize access to plimit
 * from concurrent threads within the same process.
 * Callers must be holding the proc lock to enter, return with
 * the proc lock locked
 */
static void proc_limitblock(proc_t p) {
	lck_mtx_assert(&p->p_mlock, LCK_MTX_ASSERT_OWNED);

	while (p->p_lflag & P_LLIMCHANGE) {
		p->p_lflag |= P_LLIMWAIT;
		msleep(&p->p_limit, &p->p_mlock, 0, "proc_limitblock", NULL);
	}
	p->p_lflag |= P_LLIMCHANGE;
}

/*
 * Callers must be holding the proc lock to enter, return with
 * the proc lock locked
 */
static void
proc_limitunblock(proc_t p)
{
	lck_mtx_assert(&p->p_mlock, LCK_MTX_ASSERT_OWNED);

	p->p_lflag &= ~P_LLIMCHANGE;
	if (p->p_lflag & P_LLIMWAIT) {
		p->p_lflag &= ~P_LLIMWAIT;
		wakeup(&p->p_limit);
	}
}

int
procfs_dolimit(pfsnode_t *pnp, uio_t uio, vfs_context_t ctx)
{
	struct sbuf *sb; // Figure this out
    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);
	struct plimit *limp;
	int i;

	/*
	 * Obtain a private reference to resource limits
	 */
	proc_lock(p);
	limp = proc_limitblock(p->p_limit);
	proc_rele(p);

	for (i = 0; i < RLIM_NLIMITS; i++) {
		/*
		 * Add the rlimit ident
		 */
		sbuf_printf(sb, "%s ", rlimit_ident[i]);

		/*
		 * Replace RLIM_INFINITY with -1 in the string
		 */

		/*
		 * current limit
		 */
		if (limp->pl_rlimit[i].rlim_cur == RLIM_INFINITY) {
			sbuf_printf(sb, "-1 ");
		} else {
			sbuf_printf(sb, "%llu ",
			    (unsigned long long)limp->pl_rlimit[i].rlim_cur);
		}

		/*
		 * maximum limit
		 */
		if (limp->pl_rlimit[i].rlim_max == RLIM_INFINITY) {
			sbuf_printf(sb, "-1\n");
		} else {
			sbuf_printf(sb, "%llu\n",
			    (unsigned long long)limp->pl_rlimit[i].rlim_max);
		}
	}
	proc_limitunblock(limp);

	return (0);
}

#endif /* _RLIMIT_IDENT */