/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
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

#if defined(__x86_64__)
#include <i386/vmparam.h> // for USRSTACK
#elif defined(__arm64__) || defined(__aarch64__)
#include <arm/vmparam.h>  // for USRSTACK
#endif
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
	char    path[MAXPATHLEN];
	int     ac;
	int     ec;
	char    **av;
	char    **ev;
};

int grade_binary(cpu_type_t, cpu_subtype_t, cpu_subtype_t, bool allow_simulator_binary);
int binary_grade_overrides_update(char *overrides_arg);
size_t bingrade_get_override_string(char *existing_overrides, size_t existing_overrides_bufsize);
boolean_t binary_match(cpu_type_t mask_bits, cpu_type_t req_cpu,
    cpu_subtype_t req_subcpu, cpu_type_t test_cpu,
    cpu_subtype_t test_subcpu);

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
