/*
 * symbols.c - Private KPI symbol resolution
 *
 * On ARM64 macOS, kernel memory scanning is not possible due to PAC
 * (Pointer Authentication Code) enforcement. All private symbols are
 * stubbed as NULL. Public KPI alternatives are used where available.
 *
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 */
#include <mach/kern_return.h>
#include <libkern/libkern.h>
#include <ptrauth.h>
#include "symbols.h"

/* libklookup: resolves private kernel functions from the on-disk kernelcache
 * symbol table (applying the KASLR slide), reaching symbols absent from every
 * .exports and jettisoned from the running kernel's __LINKEDIT. */
extern int klookup_resolve(const char *const *names, void **out, int count);
extern const char version[];

/* Set once at load if libklookup validates (in resolve_symbols). Code that uses
 * a klookup-resolved private symbol should gate on this. */
boolean_t procfs_klookup_ok = FALSE;

/*
 * Sign a klookup-resolved raw function address as a C function pointer so it can
 * be called under the arm64e kernel ABI (key IA + type discriminator). On
 * non-ptrauth targets this is a plain cast.
 */
#if __has_feature(ptrauth_calls)
#define KL_SIGN_FN(addr, fptype) \
    ptrauth_sign_unauthenticated((void *)(addr), ptrauth_key_function_pointer, \
        ptrauth_function_pointer_type_discriminator(fptype))
#else
#define KL_SIGN_FN(addr, fptype) ((void *)(addr))
#endif

#define SYM_INIT(sym) \
	__typeof(_##sym) _##sym = NULL

SYM_INIT(proc_best_name);
SYM_INIT(proc_starttime);
SYM_INIT(proc_issetugid);
SYM_INIT(proc_get_darwinbgstate);
SYM_INIT(psignal);
SYM_INIT(tsleep);
SYM_INIT(proc_lock);
SYM_INIT(proc_unlock);
SYM_INIT(proc_list_lock);
SYM_INIT(proc_list_unlock);
SYM_INIT(proc_iterate);
SYM_INIT(proc_pgrp);
SYM_INIT(pg_rele);
SYM_INIT(pgfind);
SYM_INIT(pgrp_lock);
SYM_INIT(pgrp_unlock);
SYM_INIT(pgrp_iterate);
SYM_INIT(tty_lock);
SYM_INIT(tty_unlock);
SYM_INIT(forkproc);
SYM_INIT(forkproc_free);
SYM_INIT(filedesc0);
SYM_INIT(fdcopy);
SYM_INIT(fdfree);
SYM_INIT(fill_taskprocinfo);
SYM_INIT(fill_taskthreadinfo);
SYM_INIT(fill_socketinfo);
SYM_INIT(proc_thread);
SYM_INIT(thread_info);
SYM_INIT(get_bsdthread_info);
SYM_INIT(bsd_threadcdir);
SYM_INIT(uthread_alloc);
SYM_INIT(convert_port_to_thread);
SYM_INIT(task_threads);
SYM_INIT(vfs_context_thread);
SYM_INIT(vn_stat);
SYM_INIT(vnode_lock);
SYM_INIT(vnode_unlock);
SYM_INIT(dead_mountp);
SYM_INIT(processor_count);
SYM_INIT(avenrun);
#if defined(__x86_64__)
SYM_INIT(tscFreq);
SYM_INIT(cpuid_info);
#endif

kern_return_t
resolve_symbols(void)
{
    /*
     * Batch-resolve the private symbols we need from the on-disk kernelcache.
     * Include "_version" as a self-test: it must resolve to &version, proving
     * the kernelcache and KASLR slide are right for this running kernel.
     */
    enum { I_VERSION, I_FILL_TASKPROCINFO, I_FILL_TASKTHREADINFO, N_SYMS };
    static const char *const names[N_SYMS] = {
        [I_VERSION]              = "_version",
        [I_FILL_TASKPROCINFO]    = "_fill_taskprocinfo",
        [I_FILL_TASKTHREADINFO]  = "_fill_taskthreadinfo",
    };
    void *addr[N_SYMS] = { NULL };

    klookup_resolve(names, addr, N_SYMS);

    if (addr[I_VERSION] != (void *)(uintptr_t)version) {
        printf("procfs: libklookup validation FAILED (_version=%p &version=%p)\n",
               addr[I_VERSION], (void *)(uintptr_t)version);
        return KERN_SUCCESS;
    }

    procfs_klookup_ok = TRUE;

    /* Sign the resolved function addresses so they are callable under PAC. */
    if (addr[I_FILL_TASKPROCINFO] != NULL) {
        _fill_taskprocinfo = KL_SIGN_FN(addr[I_FILL_TASKPROCINFO],
                                        __typeof(_fill_taskprocinfo));
    }
    if (addr[I_FILL_TASKTHREADINFO] != NULL) {
        _fill_taskthreadinfo = KL_SIGN_FN(addr[I_FILL_TASKTHREADINFO],
                                          __typeof(_fill_taskthreadinfo));
    }

    printf("procfs: libklookup OK (taskprocinfo=%d taskthreadinfo=%d)\n",
           _fill_taskprocinfo != NULL, _fill_taskthreadinfo != NULL);

    return KERN_SUCCESS;
}
