/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfsd.c
 *
 * Userspace daemon for the procfs kext. It connects to the kext's PF_SYSTEM
 * kernel control and answers requests using libproc's proc_pidinfo() - the data
 * the kext cannot obtain on arm64 (the full proc_taskinfo, per-thread info).
 * Run as root via a LaunchDaemon; it reconnects automatically across kext
 * load/unload.
 *
 *   cc -O2 -Wall -o procfsd tools/procfsd.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <spawn.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <sys/kern_control.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <libproc.h>
#include <sys/proc_info.h>
#include <mach/mach.h>

#include "../include/fs/procfs/procfs_ctl.h"

extern char **environ;

/* Boot orchestration paths. */
#define PROCFS_BUNDLE_ID  "com.beako.filesystems.procfs"
#define PROCFS_STAGER     "/usr/local/sbin/procfs_ksyms"   /* symbol-staging helper */
#define PROCFS_ARM_FLAG   "/var/db/procfs.enabled"         /* gate for auto-loading the kext */

/* Run a program to completion (best-effort). */
static void
run_to_completion(const char *path, char *const argv[])
{
    pid_t pid;
    if (posix_spawn(&pid, path, NULL, NULL, argv, environ) == 0) {
        int status;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            /* retry */
        }
    }
}

/*
 * Boot bootstrap: stage the kernel symbols (so the kext's libklookup features
 * work), then - only if armed - load the kext. The arm flag is absent by
 * default so a kext panic during development cannot boot-loop the machine; the
 * operator creates PROCFS_ARM_FLAG to enable auto-load. Mounting is left to the
 * per-user login agent (the mount lives in the user's ~/proc).
 */
static void
procfsd_bootstrap(void)
{
    if (access(PROCFS_STAGER, X_OK) == 0) {
        fprintf(stderr, "procfsd: staging kernel symbols (%s)\n", PROCFS_STAGER);
        char *argv[] = { (char *)PROCFS_STAGER, NULL };
        run_to_completion(PROCFS_STAGER, argv);
    } else {
        fprintf(stderr, "procfsd: stager %s not found; skipping symbol staging\n", PROCFS_STAGER);
    }

    if (access(PROCFS_ARM_FLAG, F_OK) == 0) {
        fprintf(stderr, "procfsd: armed (%s present) - loading kext %s\n",
            PROCFS_ARM_FLAG, PROCFS_BUNDLE_ID);
        char *argv[] = { "/usr/bin/kmutil", "load", "-b", (char *)PROCFS_BUNDLE_ID, NULL };
        run_to_completion("/usr/bin/kmutil", argv);
    } else {
        fprintf(stderr, "procfsd: not armed (%s absent); skipping kext auto-load. "
            "Create it to enable auto-load + login mount.\n", PROCFS_ARM_FLAG);
    }
}

#define PROCFS_MOUNTPOINT "/proc"

/*
 * Keep procfs mounted at /proc, as root, via the installed mount_procfs helper
 * (/sbin/mount -t procfs). A direct mount(2) from this daemon's context returns
 * EFAULT, but spawning the helper - the same path the mount(8) command uses -
 * works. /proc itself is created by /etc/synthetic.conf (see the installer).
 * Gated on the arm flag; a no-op until /proc exists and while already mounted.
 */
static void
procfsd_try_mount(void)
{
    if (access(PROCFS_ARM_FLAG, F_OK) != 0) {
        return;
    }

    struct stat st;
    if (stat(PROCFS_MOUNTPOINT, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return;                 /* /proc not present yet (needs synthetic.conf + reboot) */
    }

    struct statfs sfs;
    if (statfs(PROCFS_MOUNTPOINT, &sfs) == 0 &&
        strcmp(sfs.f_fstypename, "procfs") == 0) {
        return;                 /* already mounted */
    }

    char *argv[] = { "/sbin/mount", "-t", "procfs", "procfs", PROCFS_MOUNTPOINT, NULL };
    run_to_completion("/sbin/mount", argv);

    if (statfs(PROCFS_MOUNTPOINT, &sfs) == 0 &&
        strcmp(sfs.f_fstypename, "procfs") == 0) {
        fprintf(stderr, "procfsd: mounted procfs at %s\n", PROCFS_MOUNTPOINT);
    } else {
        fprintf(stderr, "procfsd: failed to mount %s (is mount_procfs installed?)\n",
            PROCFS_MOUNTPOINT);
    }
}

static void *
procfsd_mount_thread(void *arg)
{
    (void)arg;
    for (;;) {
        procfsd_try_mount();
        sleep(3);
    }
    return NULL;
}

/* Connect to the kext's kernel control. Returns the socket fd or -1. */
static int
connect_ctl(void)
{
    int fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (fd < 0) {
        return -1;
    }

    struct ctl_info info;
    memset(&info, 0, sizeof(info));
    strlcpy(info.ctl_name, PROCFS_CTL_NAME, sizeof(info.ctl_name));
    if (ioctl(fd, CTLIOCGINFO, &info) < 0) {
        close(fd);
        return -1;      /* control not registered yet (kext not loaded) */
    }

    struct sockaddr_ctl addr;
    memset(&addr, 0, sizeof(addr));
    addr.sc_len      = sizeof(addr);
    addr.sc_family   = AF_SYSTEM;
    addr.ss_sysaddr  = AF_SYS_CONTROL;
    addr.sc_id       = info.ctl_id;
    addr.sc_unit     = 0;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int
wait_connect(void)
{
    for (;;) {
        int fd = connect_ctl();
        if (fd >= 0) {
            return fd;
        }
        sleep(1);       /* wait for the kext to register the control */
    }
}

int
main(void)
{
    procfsd_bootstrap();        /* stage symbols, gated kext load */

    /* Keep the console user's ~/proc mounted (root; gated by the arm flag). */
    pthread_t mt;
    if (pthread_create(&mt, NULL, procfsd_mount_thread, NULL) == 0) {
        pthread_detach(mt);
    }

    int fd = wait_connect();
    fprintf(stderr, "procfsd: connected to %s\n", PROCFS_CTL_NAME);

    for (;;) {
        uint8_t rbuf[256];
        ssize_t n = recv(fd, rbuf, sizeof(rbuf), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);                  /* kext unloaded / socket error */
            fd = wait_connect();
            fprintf(stderr, "procfsd: reconnected\n");
            continue;
        }
        if (n < (ssize_t)sizeof(struct procfs_ctl_req)) {
            continue;
        }

        struct procfs_ctl_req *req = (struct procfs_ctl_req *)rbuf;
        if (req->magic != PROCFS_CTL_MAGIC) {
            continue;
        }

        uint8_t sbuf[sizeof(struct procfs_ctl_resp) + PROCFS_CTL_MAXPAYLOAD];
        struct procfs_ctl_resp *resp = (struct procfs_ctl_resp *)sbuf;
        resp->magic = PROCFS_CTL_MAGIC;
        resp->seq   = req->seq;
        resp->error = 0;
        resp->len   = 0;
        void *payload = sbuf + sizeof(*resp);

        switch (req->type) {
        case PROCFS_REQ_TASKINFO: {
            struct proc_taskinfo ti;
            int r = proc_pidinfo(req->pid, PROC_PIDTASKINFO, 0, &ti, sizeof(ti));
            if (r == (int)sizeof(ti)) {
                memcpy(payload, &ti, sizeof(ti));
                resp->len = sizeof(ti);
            } else {
                resp->error = (r < 0) ? errno : ESRCH;
            }
            break;
        }
        case PROCFS_REQ_THREADINFO: {
            /* arg is the kext's tid (== thread_id). PROC_PIDTHREADID64INFO keys
             * on thread_id, so the tid is the handle directly - no mapping. */
            struct proc_threadinfo thi;
            int r = proc_pidinfo(req->pid, PROC_PIDTHREADID64INFO, req->arg, &thi, sizeof(thi));
            if (r == (int)sizeof(thi)) {
                memcpy(payload, &thi, sizeof(thi));
                resp->len = sizeof(thi);
            } else {
                resp->error = (r < 0) ? errno : ESRCH;
            }
            break;
        }
        case PROCFS_REQ_VMSTAT: {
            vm_statistics64_data_t vm;
            mach_msg_type_number_t cnt = HOST_VM_INFO64_COUNT;
            if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                    (host_info64_t)&vm, &cnt) == KERN_SUCCESS) {
                size_t n = (size_t)cnt * sizeof(integer_t);
                if (n > PROCFS_CTL_MAXPAYLOAD) {
                    n = PROCFS_CTL_MAXPAYLOAD;
                }
                memcpy(payload, &vm, n);
                resp->len = (uint32_t)n;
            } else {
                resp->error = EIO;
            }
            break;
        }
        case PROCFS_REQ_LOADAVG: {
            double la[3] = { 0, 0, 0 };
            (void)getloadavg(la, 3);
            uint32_t out[3];
            for (int i = 0; i < 3; i++) {
                out[i] = (uint32_t)(la[i] * 100.0 + 0.5);
            }
            memcpy(payload, out, sizeof(out));
            resp->len = sizeof(out);
            break;
        }
        default:
            resp->error = EINVAL;
            break;
        }

        (void)send(fd, sbuf, sizeof(*resp) + resp->len, 0);
    }
    return 0;
}
