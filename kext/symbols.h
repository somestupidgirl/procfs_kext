#ifndef _symbols_h
#define _symbols_h

#include <sys/proc_internal.h>

#pragma mark -
#pragma mark Symbol Resolver

static void(*_proc_list_lock)(void) = NULL;
static void(*_proc_list_unlock)(void) = NULL;
static void(*_proc_fdlock_spin)(proc_t p) = NULL;
static void(*_proc_fdunlock)(proc_t p) = NULL;
static void(*_session_lock)(struct session * sess) = NULL;
static void(*_session_unlock)(struct session * sess) = NULL;
static int(*_proc_pidbsdinfo)(proc_t p, struct proc_bsdinfo * pbsd, int zombie) = NULL;
static int(*_proc_pidtaskinfo)(proc_t p, struct proc_taskinfo * ptinfo) = NULL;
static int(*_proc_pidthreadinfo)(proc_t p, uint64_t arg, bool thuniqueid, struct proc_threadinfo *pthinfo) = NULL;
static void(*_proc_iterate)(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg) = NULL;
static task_t(*_proc_task)(proc_t proc) = NULL;
static struct fdt_iterator(*_fdt_next)(proc_t p, int fd, bool only_settled) = NULL;
static errno_t(*_fill_socketinfo)(socket_t so, struct socket_info *si) = NULL;
static kern_return_t(*_task_threads)(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count) = NULL;
static kern_return_t(*_thread_info)(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count) = NULL;
static thread_t(*_convert_port_to_thread)(ipc_port_t port) = NULL;
static struct pgrp *(*_proc_pgrp)(proc_t p) = NULL;

#endif /* _symbols_h */
