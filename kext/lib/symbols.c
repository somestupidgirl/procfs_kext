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

/*
 * Raw runtime addresses of private kernel functions resolved via libklookup
 * (NULL if unavailable). These are unsigned; sign with KL_SIGN_FN at the call
 * site, using the function-pointer type the call expects.
 *
 * On arm64 the fill_taskinfo/fill_taskthreadinfo symbols are stripped from the
 * kernel entirely (see reference memory), so taskinfo/threadinfo cannot use
 * them. proc_gettty (tty) and cpu_to_processor (loadavg) survive in the symtab.
 */
void *procfs_kl_proc_gettty      = NULL;
void *procfs_kl_cpu_to_processor = NULL;

kern_return_t
resolve_symbols(void)
{
    /*
     * Resolve the private symbols we use from the staged kernel-symbol file.
     * "_version" is the slide anchor and validates by construction; klookup
     * additionally validates against "_kernel_pmap", so a non-matching staged
     * file yields NULLs and we leave the features disabled.
     */
    enum { I_VERSION, I_PROC_GETTTY, I_CPU_TO_PROCESSOR, N_SYMS };
    static const char *const names[N_SYMS] = {
        [I_VERSION]           = "_version",
        [I_PROC_GETTTY]       = "_proc_gettty",
        [I_CPU_TO_PROCESSOR]  = "_cpu_to_processor",
    };
    void *addr[N_SYMS] = { NULL };

    klookup_resolve(names, addr, N_SYMS);

    if (addr[I_VERSION] != (void *)(uintptr_t)version) {
        printf("procfs: libklookup unavailable (staged symbols missing/stale)\n");
        return KERN_SUCCESS;
    }

    procfs_klookup_ok = TRUE;
    procfs_kl_proc_gettty      = addr[I_PROC_GETTTY];
    procfs_kl_cpu_to_processor = addr[I_CPU_TO_PROCESSOR];

    printf("procfs: libklookup OK (proc_gettty=%d cpu_to_processor=%d)\n",
           procfs_kl_proc_gettty != NULL, procfs_kl_cpu_to_processor != NULL);

    return KERN_SUCCESS;
}
