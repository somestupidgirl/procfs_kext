//
//  my_kern_control.h
//  test
//
//  Created by 五千年木 on 2019/3/25.
//  Copyright © 2019年 osxkernel. All rights reserved.
//

#ifndef my_kern_control_h
#define my_kern_control_h

#include <sys/kern_control.h>
#include <sys/errno.h>
#include "define_ctl.h"
#include "define.h"
#include <sys/proc.h>

extern char g_string_buf[256];
extern char execve_string_buf[256];
extern char file_string_buf[256];

errno_t EPHandleSet( kern_ctl_ref ctlref, unsigned int unit, void *userdata, int opt, void *data, size_t len );
errno_t EPHandleGet(kern_ctl_ref ctlref, unsigned int unit, void *userdata, int opt, void *data, size_t *len);
errno_t EPHandleConnect(kern_ctl_ref ctlref, struct sockaddr_ctl *sac, void **unitinfo);
errno_t EPHandleDisconnect(kern_ctl_ref ctlref, unsigned int unit, void *unitinfo);
errno_t EPHandleWrite(kern_ctl_ref ctlref, unsigned int unit, void *userdata, mbuf_t m, int flags);

#endif /* my_kern_control_h */
