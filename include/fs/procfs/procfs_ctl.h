/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_ctl.h
 *
 * Shared wire protocol for the procfs kernel-control bridge. The kext registers
 * a PF_SYSTEM kernel control (CTL_FLAG_PRIVILEGED, root only); the userspace
 * daemon (procfsd) connects and answers requests using libproc's proc_pidinfo()
 * - the data the kext cannot obtain on its own (CPU time, faults, the full
 * proc_taskinfo, per-thread info). Included by both kext/procfs_ctl.c and
 * tools/procfsd.c.
 */
#ifndef _FS_PROCFS_PROCFS_CTL_H_
#define _FS_PROCFS_PROCFS_CTL_H_

#include <stdint.h>

#define PROCFS_CTL_NAME        "com.beako.filesystems.procfs"
#define PROCFS_CTL_MAGIC       0x50524F43u   /* 'PROC' */
#define PROCFS_CTL_MAXPAYLOAD  2048u

/* Request types (procfs_ctl_req.type). */
enum {
    PROCFS_REQ_TASKINFO   = 1,  /* payload: struct proc_taskinfo                 */
    PROCFS_REQ_THREADINFO = 2,  /* arg = tid; payload: struct proc_threadinfo    */
};

/* kext -> daemon */
struct procfs_ctl_req {
    uint32_t magic;
    uint32_t seq;       /* echoed in the response so the kext can match it */
    uint32_t type;
    int32_t  pid;
    uint64_t arg;       /* tid for thread requests, else 0 */
};

/* daemon -> kext, followed by `len` payload bytes */
struct procfs_ctl_resp {
    uint32_t magic;
    uint32_t seq;
    int32_t  error;     /* 0 on success, else an errno */
    uint32_t len;       /* payload bytes following this header (<= MAXPAYLOAD) */
};

#endif /* _FS_PROCFS_PROCFS_CTL_H_ */
