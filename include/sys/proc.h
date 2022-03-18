/*
 * Copyright (c) 2000-2018 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995, 1997 Apple Computer, Inc. All Rights Reserved */
/*-
 * Copyright (c) 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)proc.h	8.15 (Berkeley) 5/19/95
 */

#ifndef _SYS_PROC_H_
#define _SYS_PROC_H_

#include <mach/boolean.h>
#include <mach/coalition.h>     /* COALITION_NUM_TYPES */
#include <sys/appleapiopts.h>
#include <sys/cdefs.h>
#include <sys/event.h>
#include <sys/kernel_types.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/select.h>         /* For struct selinfo. */
#include <sys/time.h>
#include <uuid/uuid.h>

/* Status values. */
#define SIDL    1               /* Process being created by fork. */
#define SRUN    2               /* Currently runnable. */
#define SSLEEP  3               /* Sleeping on an address. */
#define SSTOP   4               /* Process debugging or suspension. */
#define SZOMB   5               /* Awaiting collection by parent. */

/* These flags are kept in extern_proc.p_flag. */
#define P_ADVLOCK                               0x00000001      /* Process may hold POSIX adv. lock */
#define P_CONTROLT                              0x00000002      /* Has a controlling terminal */
#define P_LP64                                  0x00000004      /* Process is LP64 */
#define P_NOCLDSTOP                             0x00000008      /* No SIGCHLD when children stop */

#define P_PPWAIT                                0x00000010      /* Parent waiting for chld exec/exit */
#define P_PROFIL                                0x00000020      /* Has started profiling */
#define P_SELECT                                0x00000040      /* Selecting; wakeup/waiting danger */
#define P_CONTINUED                             0x00000080      /* Process was stopped and continued */

#define P_SUGID                                 0x00000100      /* Has set privileges since last exec */
#define P_SYSTEM                                0x00000200      /* Sys proc: no sigs, stats or swap */
#define P_TIMEOUT                               0x00000400      /* Timing out during sleep */
#define P_TRACED                                0x00000800      /* Debugged process being traced */

#define P_DISABLE_ASLR                          0x00001000      /* Disable address space layout randomization */
#define P_WEXIT                                 0x00002000      /* Working on exiting */
#define P_EXEC                                  0x00004000      /* Process called exec. */

/* Should be moved to machine-dependent areas. */
#define P_OWEUPC                                0x00008000      /* Owe process an addupc() call at next ast. */

#define P_AFFINITY                              0x00010000      /* xxx */
#define P_TRANSLATED                            0x00020000      /* xxx */
#define P_CLASSIC                               P_TRANSLATED    /* xxx */

#define P_DELAYIDLESLEEP                        0x00040000     /* Process is marked to delay idle sleep on disk IO */
#define P_CHECKOPENEVT                          0x00080000      /* check if a vnode has the OPENEVT flag set on open */

#define P_DEPENDENCY_CAPABLE                    0x00100000      /* process is ok to call vfs_markdependency() */
#define P_REBOOT                                0x00200000      /* Process called reboot() */
#define P_RESV6                                 0x00400000      /* used to be P_TBE */
#define P_RESV7                                 0x00800000      /* (P_SIGEXC)signal exceptions */

#define P_THCWD                                 0x01000000      /* process has thread cwd  */
#define P_RESV9                                 0x02000000      /* (P_VFORK)process has vfork children */
#define P_ADOPTPERSONA                          0x04000000      /* process adopted a persona (used to be P_NOATTACH) */
#define P_RESV11                                0x08000000      /* (P_INVFORK) proc in vfork */

#define P_NOSHLIB                               0x10000000      /* no shared libs are in use for proc */
                                                                /* flag set on exec */
#define P_FORCEQUOTA                            0x20000000      /* Force quota for root */
#define P_NOCLDWAIT                             0x40000000      /* No zombies when chil procs exit */
#define P_NOREMOTEHANG                          0x80000000      /* Don't hang on remote FS ops */

#define P_INMEM                                 0               /* Obsolete: retained for compilation */
#define P_NOSWAP                                0               /* Obsolete: retained for compilation */
#define P_PHYSIO                                0               /* Obsolete: retained for compilation */
#define P_FSTRACE                               0               /* Obsolete: retained for compilation */
#define P_SSTEP                                 0               /* Obsolete: retained for compilation */

#define P_DIRTY_TRACK                           0x00000001      /* track dirty state */
#define P_DIRTY_ALLOW_IDLE_EXIT                 0x00000002      /* process can be idle-exited when clean */
#define P_DIRTY_DEFER                           0x00000004      /* defer initial opt-in to idle-exit */
#define P_DIRTY                                 0x00000008      /* process is dirty */
#define P_DIRTY_SHUTDOWN                        0x00000010      /* process is dirty during shutdown */
#define P_DIRTY_TERMINATED                      0x00000020      /* process has been marked for termination */
#define P_DIRTY_BUSY                            0x00000040      /* serialization flag */
#define P_DIRTY_MARKED                          0x00000080      /* marked dirty previously */
#define P_DIRTY_AGING_IN_PROGRESS               0x00000100      /* aging in one of the 'aging bands' */
#define P_DIRTY_LAUNCH_IN_PROGRESS              0x00000200      /* launch is in progress */
#define P_DIRTY_DEFER_ALWAYS                    0x00000400      /* defer going to idle-exit after every dirty->clean transition.
                                                                 * For legacy jetsam policy only. This is the default with the other policies.*/
#define P_DIRTY_IS_DIRTY                        (P_DIRTY | P_DIRTY_SHUTDOWN)
#define P_DIRTY_IDLE_EXIT_ENABLED               (P_DIRTY_TRACK|P_DIRTY_ALLOW_IDLE_EXIT)

__BEGIN_DECLS

extern proc_t kernproc;

extern int          proc_is_classic(proc_t p);

extern proc_t       current_proc(void);

extern int          msleep(void *chan, lck_mtx_t *mtx, int pri, const char *wmesg, struct timespec * ts );
extern void         wakeup(void *chan);
extern void         wakeup_one(caddr_t chan);

/* proc kpis */
/* this routine returns the pid of the current process */
extern int          proc_selfpid(void);

/* this routine returns the pid of the parent of the current process */
extern int          proc_selfppid(void);

/* this routine returns sends a signal signum to the process identified by the pid */
extern void         proc_signal(int pid, int signum);

/* this routine checks whether any signal identified by the mask are pending in the  process identified by the pid. The check is  on all threads of the process. */
extern int          proc_issignal(int pid, sigset_t mask);

/* this routine returns 1 if the pid1 is inferior of pid2 */
extern int          proc_isinferior(int pid1, int pid2);

/*
 * this routine copies the process's name of the executable to the passed in buffer. It
 * is always null terminated. The size of the buffer is to be passed in as well. This
 * routine is to be used typically for debugging
 */
extern void         proc_name(int pid, char * buf, int size);

/* This routine is simillar to proc_name except it returns for current process */
extern void         proc_selfname(char * buf, int size);

/* find a process with a given pid. This comes with a reference which needs to be dropped by proc_rele */
extern proc_t       proc_find(int pid);

/* find a process with a given process identity */
extern proc_t       proc_find_ident(struct proc_ident const *i);

/* returns a handle to current process which is referenced. The reference needs to be dropped with proc_rele */
extern proc_t       proc_self(void);

/* releases the held reference on the process */
extern int          proc_rele(proc_t p);

/* returns the pid of the given process */
extern int          proc_pid(proc_t);

/* returns the pid of the parent of a given process */
extern int          proc_ppid(proc_t);

/* returns the original pid of the parent of a given process */
extern int          proc_original_ppid(proc_t);

/* returns 1 if the process is marked for no remote hangs */
extern int          proc_noremotehang(proc_t);

/* returns 1 if the process is marked for force quota */
extern int          proc_forcequota(proc_t);

/* returns 1 if the process is chrooted */
extern int          proc_chrooted(proc_t);

/* this routine returns 1 if the process is running with a 64bit address space, else 0 */
extern int          proc_is64bit(proc_t);

/* this routine returns 1 if the process is running with a 64bit register state, else 0 */
extern int          proc_is64bit_data(proc_t);

/* is this process exiting? */
extern int          proc_exiting(proc_t);

/* this routine returns error if the process is not one with super user privileges */
extern int          proc_suser(proc_t p);

/*!
 *  @function proc_selfpgrpid
 *  @abstract Get the process group id for the current process, as with proc_pgrpid().
 *  @return pgrpid of current process.
 */
extern pid_t        proc_selfpgrpid(void);

/*!
 *  @function proc_pgrpid
 *  @abstract Get the process group id for the passed-in process.
 *  @param p Process whose pgrpid to grab.
 *  @return pgrpid for "p".
 */
extern pid_t        proc_pgrpid(proc_t p);

/*!
 *  @function proc_sessionid
 *  @abstract Get the process session id for the passed-in process.
 *  @param p Process whose session id to grab.
 *  @return session id of current process.
 */
extern pid_t        proc_sessionid(proc_t p);

/*
 *  System call filtering for BSD syscalls, mach traps and kobject routines.
 */
#define SYSCALL_MASK_UNIX 0
#define SYSCALL_MASK_MACH 1
#define SYSCALL_MASK_KOBJ 2

#define SYSCALL_FILTER_CALLBACK_VERSION 1

typedef int (*syscall_filter_cbfunc_t)(proc_t p, int num);
typedef int (*kobject_filter_cbfunc_t)(proc_t p, int msgid, int idx);

struct syscall_filter_callbacks {
	int version;
	const syscall_filter_cbfunc_t unix_filter_cbfunc;
	const syscall_filter_cbfunc_t mach_filter_cbfunc;
	const kobject_filter_cbfunc_t kobj_filter_cbfunc;
};

typedef struct syscall_filter_callbacks * syscall_filter_cbs_t;

__END_DECLS

/*
 * Values for pid_shutdown_sockets
 */
#define SHUTDOWN_SOCKET_LEVEL_DISCONNECT_SVC            0x00000001
#define SHUTDOWN_SOCKET_LEVEL_DISCONNECT_ALL            0x00000002
#define SHUTDOWN_SOCKET_LEVEL_DISCONNECT_INTERNAL       0x10000000
#define SHUTDOWN_SOCKET_LEVEL_NECP                      0x20000000
#define SHUTDOWN_SOCKET_LEVEL_CONTENT_FILTER            0x40000000

/*
 * Entitlement to allow non-root processes to suspend/resume any task
 */
#define PROCESS_RESUME_SUSPEND_ENTITLEMENT "com.apple.private.process.suspend-resume.any"

#endif  /* !_SYS_PROC_H_ */
