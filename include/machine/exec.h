/*
 * Copyright (c) 2000-2018 Apple Inc. All rights reserved.
 * Copyright (c) 2022 Sunneva Jonsdottir
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
/*
 * Copyright 1995 NeXT Computer, Inc. All rights reserved.
 */
#ifndef _BSD_MACHINE_EXEC_H_
#define _BSD_MACHINE_EXEC_H_

#include <i386/vmparam.h> // for USRSTACK
#include <sys/param.h>
#include <stdbool.h>

/*
 * Description:
 *      XNU's equivalent of the ps_strings structure in other BSDs.
 *
 * From NetBSD:
 *      The following structure is found at the top of the user stack of each
 *      user process. The ps program uses it to locate argv and environment
 *      strings. Programs that wish ps to display other information may modify
 *      it; normally ps_argvstr points to argv[0], and ps_nargvstr is the same
 *      as the program's argc. The fields ps_envstr and ps_nenvstr are the
 *      equivalent for the environment.
 *
 * Notes:
 *      NetBSD does not have the "path[MAXPATHLEN] field."
 *
 *      The fields in NetBSD correspond here as follows:
 *
 *      NetBSD:               XNU:
 *      -------------------------------
 *      char **ps_argvstr;  | char **av;
 *      int    ps_nargvstr; | int ac;
 *      char **ps_envstr;   | char **ev;
 *      int    ps_nenvstr;  | int ec;
 */
struct exec_info {
	char      path[MAXPATHLEN];
	int       ac;              /* the number of argument strings */
	int       ec;              /* the number of environment strings */
	char    **av;              /* first of 0 or more argument strings */
	char    **ev;              /* first of 0 or more environment strings */
};

/*
 * Description:
 *      Macro PS_STRINGS ported from NetBSD to complement XNU's exec_info structure.
 *
 * From NetBSD:
 *      Address of exec_info structure. We only use this as a default in user space.
 */
#define EXEC_INFO \
    ((struct exec_info *)(USRSTACK - sizeof(struct exec_info)))

#define PS_STRINGS \
    EXEC_INFO

#endif /* _BSD_MACHINE_EXEC_H_ */
