//
//  syscall.h
//  test
//
//  Created by 五千年木 on 2019/3/20.
//  Copyright © 2019年 osxkernel. All rights reserved.
//

#ifndef syscall_h
#define syscall_h

#include <mach-o/loader.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/mach_port.h>
#include <mach/task.h>

#include <kern/task.h>
#include <kern/clock.h>

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/proc.h>

#include <libkern/OSMalloc.h>
#include <libkern/version.h>

#include <IOKit/IOLib.h>
#include <string.h>

#include "my_kern_control.h"
#include "sysent.h"
#include "mysys_args.h"
extern void *syscall_table[521];

//void *syscall_table[521];

int hook_kill(proc_t cp, struct kill_args *uap, __unused int32_t *retval);
int hook_open(proc_t cp, struct open_args *uap, int32_t *retval);
int hook_rename(struct proc *, struct rename_args *, int *);
int hook_copyfile(struct proc *, struct copyfile_args *, int *);

int hook_execve(struct proc *, struct execve_args *, int *);
int hook_posix_spawn(struct proc *, struct posix_spawn_args *, int *);
int hook__mac_execve(struct proc *cp, struct __mac_execve_args *uap, int *retval);

int hook_read(struct proc *cp, struct read_args *uap, user_ssize_t *retval);
int hook_write(struct proc *cp, struct writev_args *uap, user_ssize_t *retval);
int hook_delete(struct proc *cp, struct delete_args *uap, int *retval);

#endif /* syscall_h */
