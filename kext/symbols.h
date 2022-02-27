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
static int(*_vn_stat)(struct vnode *vp, void * sb, kauth_filesec_t *xsec, int isstat64, int needsrealdev, vfs_context_t ctx) = NULL;
static uint64_t (*_vnode_get_va_fsid)(struct vnode_attr *vap) = NULL;
static errno_t(*_fill_socketinfo)(socket_t so, struct socket_info *si) = NULL;
static kern_return_t(*_task_threads)(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count) = NULL;
static kern_return_t(*_thread_info)(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count) = NULL;
static thread_t(*_convert_port_to_thread)(ipc_port_t port) = NULL;

// #define f_flag fp_glob->fg_flag
// #define f_type fp_glob->fg_ops->fo_type
// #define f_cred fp_glob->fg_cred
// #define f_ops fp_glob->fg_ops
// #define f_offset fp_glob->fg_offset
// #define f_data fp_glob->fg_data

#define _f_flag 	(uint32_t) SymbolLookup("_f_flag")
#define _f_type 	(int32_t) SymbolLookup("_f_type")
#define _f_cred 	SymbolLookup("_f_cred")
#define _f_ops 		SymbolLookup("_f_ops")
#define _f_offset 	(off_t) SymbolLookup("_f_offset")
#define _f_data 	SymbolLookup("_f_data")

#endif /* _symbols_h */
