/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_fpregs.c
 *
 * Process floating-point register node (/proc/<pid>/fpregs).
 *
 * Named after NetBSD's procfs_dofpregs() (sys/miscfs/procfs/procfs_machdep.c)
 * for cross-reference. NetBSD dumps a machine-dependent `struct fpreg`; macOS
 * has no such struct, so this node emits the platform's native Mach floating-
 * point / SIMD state - the direct analog - obtained with thread_get_state():
 *
 *   arm64:  arm_neon_state64_t    (flavor ARM_NEON_STATE64)
 *   x86_64: x86_float_state64_t    (flavor x86_FLOAT_STATE64)
 *
 * As with regs, the state is reported for the process's representative thread
 * (the first on its p_uthlist), matching NetBSD's per-process fpregs node.
 *
 * A human-readable Linux-style dump is provided separately, guarded, in
 * procfs_linux.c for the planned native-vs-linux compatibility mode.
 */
#include <stdint.h>
#include <string.h>

#include <kern/thread.h>
#include <mach/thread_act.h>
#include <mach/thread_status.h>
#if defined(__arm64__) || defined(__aarch64__)
#include <mach/arm/thread_status.h>
#elif defined(__x86_64__)
#include <mach/i386/thread_status.h>
#endif
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/uio.h>

#include <fs/procfs/procfs.h>

int
procfs_dofpregs(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    if (uio_rw(uio) != UIO_READ) {
        return EOPNOTSUPP;       /* the node is read-only */
    }

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p == PROC_NULL) {
        return ESRCH;
    }

    thread_t thread = procfs_get_representative_thread(p);
    if (thread == THREAD_NULL) {
        proc_rele(p);
        return ESRCH;            /* zombie/system process with no live thread */
    }

#if defined(__arm64__) || defined(__aarch64__)
    arm_neon_state64_t     state;
    mach_msg_type_number_t count = ARM_NEON_STATE64_COUNT;
    const int              flavor = ARM_NEON_STATE64;
#elif defined(__x86_64__)
    x86_float_state64_t    state;
    mach_msg_type_number_t count = x86_FLOAT_STATE64_COUNT;
    const int              flavor = x86_FLOAT_STATE64;
#else
    proc_rele(p);
    return ENOTSUP;
#endif

#if defined(__arm64__) || defined(__aarch64__) || defined(__x86_64__)
    memset(&state, 0, sizeof(state));
    kern_return_t kr = thread_get_state((thread_act_t)thread, flavor,
                                        (thread_state_t)&state, &count);
    if (kr != KERN_SUCCESS) {
        proc_rele(p);
        return EIO;
    }

    /* count is returned in natural_t (32-bit) words; clamp to the struct. */
    int len = (int)(count * sizeof(natural_t));
    if (len > (int)sizeof(state)) {
        len = (int)sizeof(state);
    }

    int error = procfs_copy_data((const char *)&state, len, uio);
    proc_rele(p);
    return error;
#endif
}
