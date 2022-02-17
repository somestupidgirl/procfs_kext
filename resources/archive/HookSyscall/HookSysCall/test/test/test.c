//
//  test.c
//  test
//  Darwin Kernel Version 16.7.0: Thu Jun 15 17:36:27 PDT 2017; root:xnu-3789.70.16~2/RELEASE_X86_64
//  Created by 五千年木 on 2018/12/24.
//  Copyright © 2018年 osxkernel. All rights reserved.
//
//
//#include <mach-o/loader.h>
//#include <mach/mach_types.h>
//#include <mach/message.h>
//#include <mach/mach_port.h>
//#include <mach/task.h>
//
//#include <kern/task.h>
//#include <kern/clock.h>
//
//#include <sys/systm.h>
//#include <sys/kernel.h>
//#include <sys/sysctl.h>
//#include <sys/types.h>
//#include <sys/proc.h>
//
//#include <libkern/OSMalloc.h>
//#include <libkern/version.h>
//
//#include <IOKit/IOLib.h>

#include "my_kern_control.h"
#include "test.h"
#include "syscall.h"
#include "resolver.h"
#include "macho_utils.h"
#include "define.h"

struct kern_ctl_reg  ep_ctl; // Initialize control
kern_ctl_ref     kctlref;
static boolean_t g_register = FALSE;

uint8_t syscall_table_bool[521] = {NULL};
void * syscall_table_hook[521] = {NULL};

#define MSR_EFER        0xc0000080 /* extended feature register */
#define MSR_STAR        0xc0000081 /* legacy mode SYSCALL target */
#define MSR_LSTAR       0xc0000082 /* long mode SYSCALL target */
#define MSR_CSTAR       0xc0000083 /* compat mode SYSCALL target */

#define INVALID_VADDR   ((uintptr_t)-1)

#if !defined(assert)
#   define assert(cond)    \
((void) ((cond) ? 0 : panic("assertion failed: %s", # cond)))
#endif

OSMallocTag g_tag = NULL;
lck_grp_t* g_lock_group = NULL;

// Traced task context
static task_t g_task = NULL;
static int g_hooked = 0;
static int32_t g_pid = 0;       // PID we will protect, set through sysctl node
static int g_unhook = 0;        // Dummy sysctl node var to unhook everything before exiting


// Hooked syscall tables
static void* g_sysent_table = NULL;
static void* g_mach_trap_table = NULL;

// Private kernel symbols manually resolved on kext start
static task_t(*proc_task)(proc_t) = NULL;
static ipc_space_t(*get_task_ipcspace)(task_t) = NULL;
static task_t(*port_name_to_task)(mach_port_name_t) = NULL;

static lck_mtx_t* g_task_lock = NULL;

static void* sysent_get_call(int callnum) {
    switch(version_major) {
        case 16: return ((struct sysent_sierra*)g_sysent_table)[callnum].sy_call;
        case 14: return ((struct sysent_yosemite*)g_sysent_table)[callnum].sy_call;
        case 13: return ((struct sysent_mavericks*)g_sysent_table)[callnum].sy_call;
        default: return ((struct sysent*)g_sysent_table)[callnum].sy_call;
    }
}

static void sysent_set_call(int callnum, void* sy_call) {
    switch(version_major) {
        case 16: ((struct sysent_sierra*)g_sysent_table)[callnum].sy_call = sy_call; break;
        case 14: ((struct sysent_yosemite*)g_sysent_table)[callnum].sy_call = sy_call; break;
        case 13: ((struct sysent_mavericks*)g_sysent_table)[callnum].sy_call = sy_call; break;
        default: ((struct sysent*)g_sysent_table)[callnum].sy_call = sy_call; break;
    }
}

static void* sysent_hook_call(int callnum, void* hook) {
    void* orig = sysent_get_call(callnum);
    sysent_set_call(callnum, hook);
    return orig;
}

static void* mach_table_get_trap(int trapnum) {
    if (version_major >= 13) {
        return ((mach_trap_mavericks_t*)g_mach_trap_table)[trapnum].mach_trap_function;
    } else {
        return ((mach_trap_t*)g_mach_trap_table)[trapnum].mach_trap_function;
    }
}

static void mach_table_set_trap(int trapnum, void* trap_function) {
    if (version_major >= 13) {
        ((mach_trap_mavericks_t*)g_mach_trap_table)[trapnum].mach_trap_function = trap_function;
    } else {
        ((mach_trap_t*)g_mach_trap_table)[trapnum].mach_trap_function = trap_function;
    }
}

static void* mach_table_hook_trap(int trapnum, void* hook) {
    void* orig = mach_table_get_trap(trapnum);
    mach_table_set_trap(trapnum, hook);
    return orig;
}

static uint64_t rdmsr(uint32_t index)
{
    uint32_t lo=0, hi=0;
    __asm__ __volatile__ ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(index));
    return (((uint64_t)hi) << 32) | ((uint64_t)lo);
}

// Clear CR0 page read only protection bit
static void disable_vm_protection(void)
{
    __asm__ __volatile__(
                         "cli    \n\t" \
                         "mov    %%cr0, %%rax \n\t" \
                         "and    $0xfffffffffffeffff, %%rax \n\t" \
                         "mov    %%rax, %%cr0 \n\t" \
                         "sti    \n\t"
                         :::"rax"
                         );
}

// Set CR0 page read only protection bit
static void enable_vm_protection(void)
{
    __asm__ __volatile__(
                         "cli    \n\t" \
                         "mov    %%cr0, %%rax \n\t" \
                         "or     $0x10000, %%rax \n\t" \
                         "mov    %%rax, %%cr0 \n\t" \
                         "sti    \n\t"
                         :::"rax"
                         );
}

// Finds and returns 64bit loaded kernel base address or INVALID_VADDR if failed
static uintptr_t find_kernel_base(void)
{
    // 通过一个巧妙的手段找到kernel的基地址
    // In case of ASLR kernel find real kernel base.
    // For that dump MSR_LSTAR which contains a pointer to kernel syscall handler
    uint64_t ptr = rdmsr(MSR_LSTAR);
    
    // Round up to next page boundary - kernel should start at a page boundary ASLR or no ALSR
    ptr = ptr & ~PAGE_MASK_64;
    while (ptr) {
        if (*(uint32_t*)ptr == MH_MAGIC_64) {
            return ptr;
        }
        
        ptr -= PAGE_SIZE;
    }
    
    return INVALID_VADDR;
}



// Matches sysent table in memory at given address
static int is_sysent_table(uintptr_t addr)
{
#define sysent_verify(_sysent)              \
((_sysent)[SYS_exit].sy_narg == 1 &&    \
(_sysent)[SYS_fork].sy_narg == 0 &&    \
(_sysent)[SYS_read].sy_narg == 3 &&    \
(_sysent)[SYS_wait4].sy_narg == 4 &&   \
(_sysent)[SYS_ptrace].sy_narg == 4)
    
    if (version_major == 16) {
        struct sysent_sierra* sysent = (struct sysent_sierra*)addr;
        return sysent_verify(sysent);
    } else if (version_major == 14) {
        struct sysent_yosemite* sysent = (struct sysent_yosemite*)addr;
        return sysent_verify(sysent);
    } else if (version_major == 13) {
        struct sysent_mavericks* sysent = (struct sysent_mavericks*)addr;
        return sysent_verify(sysent);
    } else {
        struct sysent* sysent = (struct sysent*)addr;
        return sysent_verify(sysent);
    }
    
#undef sysent_verify
    return FALSE;
}

// Matches mach trap table in memory at given address
static int is_mach_trap_table(uintptr_t addr)
{
#define traps_verify(_traps)                                \
((_traps)[0].mach_trap_arg_count == 0 &&                \
(_traps)[1].mach_trap_arg_count == 0 &&                \
(_traps)[MACH_MSG_TRAP].mach_trap_arg_count == 7 &&    \
(_traps)[MACH_MSG_OVERWRITE_TRAP].mach_trap_arg_count == 8)
    
    if (version_major >= 13) {
        mach_trap_mavericks_t* res = (mach_trap_mavericks_t*)addr;
        return traps_verify(res);
    } else {
        mach_trap_t* res = (mach_trap_t*)addr;
        return traps_verify(res);
    }
    
#undef traps_verify
    return FALSE;
}

// Search kernel data segment for BSD sysent table and mach trap table
static int find_syscall_tables(const struct segment_command_64* dataseg, void** psysent, void** pmach_traps)
{
    assert(dataseg);
    assert(psysent);
    assert(pmach_traps);
    
    void* sysent = NULL;
    void* mach_traps = NULL;
    
    uintptr_t addr = dataseg->vmaddr;
    uint64_t size = dataseg->vmsize;
    
    while(size != 0) {
        
        if (!sysent && is_sysent_table(addr)) {
            sysent = (void*)addr;
        }
        
        if (!mach_traps && is_mach_trap_table(addr)) {
            mach_traps = (void*)addr;
        }
        
        if (sysent && mach_traps) {
            *psysent = sysent;
            *pmach_traps = mach_traps;
            return TRUE;
        }
        
        addr++;
        size--;
    }
    
    return FALSE;
}

//
// Mach hooks
//

static mach_msg_return_t (*old_mach_msg_trap)(void* args) = NULL;
static mach_msg_return_t (*old_mach_msg_overwrite_trap)(void* args) = NULL;
static mach_msg_return_t (*g_mach_msg_trap)(void* args) = NULL;
static mach_msg_return_t (*g_mach_msg_overwrite_trap)(void* args) = NULL;

#define MIG_TASK_TERMINATE_ID 3401 /* Taken from osfmk/mach/task.defs */

struct mach_msg_overwrite_trap_args {
    PAD_ARG_(user_addr_t, msg);
    PAD_ARG_(mach_msg_option_t, option);
    PAD_ARG_(mach_msg_size_t, send_size);
    PAD_ARG_(mach_msg_size_t, rcv_size);
    PAD_ARG_(mach_port_name_t, rcv_name);
    PAD_ARG_(mach_msg_timeout_t, timeout);
    PAD_ARG_(mach_port_name_t, notify);
    PAD_ARG_8
    PAD_ARG_(user_addr_t, rcv_msg);  /* Unused on mach_msg_trap */
};

// User mode message header definition differs from in-kernel one
typedef	struct
{
    mach_msg_bits_t     msgh_bits;
    mach_msg_size_t     msgh_size;
    __darwin_natural_t	msgh_remote_port;
    __darwin_natural_t	msgh_local_port;
    __darwin_natural_t	msgh_voucher_port;
    mach_msg_id_t		msgh_id;
} mach_user_msg_header_t;

mach_msg_return_t mach_msg_trap_common(struct mach_msg_overwrite_trap_args *args, mach_msg_return_t(*orig_handler)(void* args))
{
    if (!g_task || !(args->option & MACH_SEND_MSG)) {
        return orig_handler(args);
    }
    
    mach_user_msg_header_t hdr;
    if (args->send_size < sizeof(hdr)) {
        return MACH_SEND_MSG_TOO_SMALL; // "Sorry, your message is too small for this rootkit to process correctly"
    }
    
    copyin(args->msg, &hdr, sizeof(hdr));
    task_t remote_task = port_name_to_task(hdr.msgh_remote_port);
    if (g_task == remote_task) {
        // TODO: also check if this is a task kernel port
        printf("my_mach_msg_trap: blocking task_terminate\n");
        return MACH_SEND_INVALID_RIGHT;
    }
    
    return orig_handler(args);
}

// mach_msg_trap hook
mach_msg_return_t my_mach_msg_trap(struct mach_msg_overwrite_trap_args *args)
{
    return mach_msg_trap_common(args, g_mach_msg_trap);
}

// mach_msg_overwrite_trap hook
mach_msg_return_t my_mach_msg_overwrite_trap(struct mach_msg_overwrite_trap_args *args)
{
    return mach_msg_trap_common(args, g_mach_msg_overwrite_trap);
}

//
// BSD kill(2) hook
//

//static int(*old_orig_kill)(proc_t cp, void *uap, __unused int32_t *retval) = NULL;
//static int(*g_orig_kill)(proc_t cp, void *uap, __unused int32_t *retval) = NULL;


//int my_kill(proc_t cp, struct kill_args *uap, __unused int32_t *retval)
//{
//    // Negative pid is a killpg case
//    pid_t pid = (uap->pid > 0 ? uap->pid : -uap->pid);
//    
//    if (!g_pid || (pid != g_pid)) {
//        return g_orig_kill(cp, uap, retval);
//    }
//    
//    printf("signal %d from pid %d to pid %d, posix %d\n", uap->signum, proc_pid(cp), uap->pid, uap->posix);
//    
//    // TODO: process cannot ignore or handle SIGKILL so we intercept it here.
//    // However there are other signals that will terminate a process if it doesn't handle or ignore these signals (i.e. SIGTERM)
//    // We don't handle those here for now.
//    if (uap->signum == SIGKILL || uap->signum == SIGTERM) {
//        printf("blocking SIGKILL\n");
//        return EPERM;
//    }
//    
//    return g_orig_kill(cp, uap, retval);
//}

//
// Entry and init
//

// kext uses sysctl nodes to communicate with the client:
// 'debug.killhook.pid' - set 32bit pid value for process to protect
// 'debug.killhook.unhook' - set to 1 to unhook all syscalls before unloading kext
// 'debug.killhook.hooked' - set to 1 to hook all syscalls after loading kext

static int sysctl_killhook_hooked SYSCTL_HANDLER_ARGS;
static int sysctl_killhook_pid SYSCTL_HANDLER_ARGS;
static int sysctl_killhook_unhook SYSCTL_HANDLER_ARGS;

SYSCTL_NODE(_debug, OID_AUTO, killhook, CTLFLAG_RW, 0, "kill hook API");


SYSCTL_PROC(_debug_killhook, OID_AUTO, hooked, (CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE), &g_hooked, 0, sysctl_killhook_hooked, "I", "");

SYSCTL_PROC(_debug_killhook, OID_AUTO, pid, (CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE), &g_pid, 0, sysctl_killhook_pid, "I", "");

SYSCTL_PROC(_debug_killhook, OID_AUTO, unhook, (CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE), &g_unhook, 0, sysctl_killhook_unhook, "I", "");



static int sysctl_killhook_hooked(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req)
{
    int res = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
    if (g_hooked)
    {
        disable_vm_protection();
        {
            if(syscall_table_bool[g_hooked] == 0x66){
                LOG_ERROR("syscall %d have hooked.",g_hooked);
            }else{
                if(g_hooked == 3 || g_hooked == 4){
                    syscall_table[g_hooked] = sysent_hook_call(g_hooked, (sy_call64_t*)syscall_table_hook[g_hooked]);
                    syscall_table_bool[g_hooked] = 0x66;
                }else{
                    syscall_table[g_hooked] = sysent_hook_call(g_hooked, (sy_call_t*)syscall_table_hook[g_hooked]);
                    syscall_table_bool[g_hooked] = 0x66;
                }
            }
        }
        enable_vm_protection();
        LOG_INFO("Hook Success.");
        g_hooked = 0;
    }
    
    return res;
}


static int sysctl_killhook_pid(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req)
{
    //int32_t curPid = g_pid;
    int res = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
//    
//    if (g_pid != curPid) {
//        
//        proc_t proc = proc_find(g_pid);
//        if (proc) {
//            g_task = proc_task(proc);
//            proc_rele(proc);
//            LOG_INFO("PID changed to %d, task %p", g_pid, g_task);
//        }
//    }
    if(g_pid){
        
        disable_vm_protection();
        {
            for(uint32_t index = 0;index < 521;index++){
                if(syscall_table_bool[index] == 0x66){
                    if(index == 3 || index == 4)
                        sysent_set_call(index, (sy_call64_t*)syscall_table[index]);
                    else
                        sysent_set_call(index, (sy_call_t*)syscall_table[index]);
                    syscall_table[index] = NULL;
                    syscall_table_bool[index] = NULL;
                }
            }
            mach_table_set_trap(MACH_MSG_OVERWRITE_TRAP, g_mach_msg_overwrite_trap);
            mach_table_set_trap(MACH_MSG_TRAP, g_mach_msg_trap);
        }
        enable_vm_protection();
        LOG_INFO("UnHook All.");
        g_pid = 0;
    }
    return res;
}

static int syscalls_hooked(int g_index)
{
    if(syscall_table_bool[g_index] == 0x66)return true;
    return false;
//    return (mach_table_get_trap(MACH_MSG_OVERWRITE_TRAP) == my_mach_msg_overwrite_trap) &&
//    (mach_table_get_trap(MACH_MSG_TRAP) == my_mach_msg_trap);//&&
           // ((sysent_get_call(SYS_kill) == hook_kill));
}


static int syscalls_hooked_all(void)
{
        return (mach_table_get_trap(MACH_MSG_OVERWRITE_TRAP) == my_mach_msg_overwrite_trap) &&
        (mach_table_get_trap(MACH_MSG_TRAP) == my_mach_msg_trap);
}

static int sysctl_killhook_unhook(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req)
{
    int res = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
    if (g_unhook && syscalls_hooked(g_unhook))
    {
        disable_vm_protection();
        {
            if(syscall_table_bool[g_unhook] == 0x66){
                if(g_unhook == 3 || g_unhook == 4){
                    sysent_set_call(g_unhook, (sy_call64_t*)syscall_table[g_unhook]);
                    syscall_table[g_unhook] = NULL;
                    syscall_table_bool[g_unhook] = NULL;
                }else{
                    sysent_set_call(g_unhook, (sy_call_t*)syscall_table[g_unhook]);
                    syscall_table[g_unhook] = NULL;
                    syscall_table_bool[g_unhook] = NULL;
                }
            }
        }
        enable_vm_protection();
        g_unhook = 0;
        LOG_INFO("unHook Success.");
    }
    
    return res;
}

kern_return_t test_start(kmod_info_t * ki, void *d)
{
    
    g_tag = OSMalloc_Tagalloc("test.kext", OSMT_DEFAULT);
    if (!g_tag) {
        LOG_ERROR("Failed to allocate OSMalloc tag ");
        return KERN_FAILURE;
    }
    
    g_lock_group = lck_grp_alloc_init("test.kext", LCK_GRP_ATTR_NULL);
    if (!g_lock_group) {
        LOG_ERROR("Failed to create lock group ");
        return KERN_FAILURE;
    }
    
    g_task_lock = lck_mtx_alloc_init(g_lock_group, LCK_ATTR_NULL);
    if (!g_task_lock) {
        LOG_ERROR("Failed to create lock group ");
        return KERN_FAILURE;
    }
    LOG_INFO("moonAgirl[test_start] start.");

    
    struct mach_header_64* kernel_hdr = (struct mach_header_64*)find_kernel_header();
    if (kernel_hdr->magic != MH_MAGIC_64) {
        LOG_ERROR("Wrong kernel header ");
        return KERN_FAILURE;
    }
    uintptr_t kernel_base = (uintptr_t)kernel_hdr;
    
    LOG_INFO("moonAgirl[test_start] version_major @ %d ", version_major);
    LOG_INFO("moonAgirl[test_start] kernel base @ %p ", kernel_base);
    LOG_INFO("moonAgirl[test_start] kernel hdr @ %p ", kernel_hdr);
    
    
    proc_task = kernel_find_symbol("_proc_task");
    get_task_ipcspace = kernel_find_symbol("_get_task_ipcspace");
    port_name_to_task = kernel_find_symbol("_port_name_to_task");
    
    if (!proc_task) LOG_ERROR("moonAgirl[test_start] proc_task. ");
    LOG_INFO("moonAgirl[test_start] proc_task:%p", proc_task);
    if (!get_task_ipcspace) LOG_ERROR("moonAgirl[test_start] get_task_ipcspace. ");
    LOG_INFO("moonAgirl[test_start] get_task_ipcspace:%p", get_task_ipcspace);
    if (!port_name_to_task) LOG_ERROR("moonAgirl[test_start] port_name_to_task. ");
    LOG_INFO("moonAgirl[test_start] port_name_to_task:%p", port_name_to_task);
    
    g_sysent_table = find_sysent_table();
    if(!g_sysent_table){
        LOG_ERROR("Can't find g_sysent_table ");
        return KERN_FAILURE;
    }
    LOG_INFO("Find g_sysent_table:%p ",g_sysent_table);
    
    g_mach_trap_table = find_mach_tables();
    if(!g_mach_trap_table){
        LOG_ERROR("Can't find syscall g_mach_trap_table");
        return KERN_FAILURE;
    }
    LOG_INFO("Find g_mach_trap_table:%p ",g_mach_trap_table);
    
    LOG_INFO("hook_kill:%p.",hook_kill);
    LOG_INFO("hook_open:%p.",hook_open);
    LOG_INFO("hook_rename:%p.",hook_rename);
    LOG_INFO("hook_copyfile:%p.",hook_copyfile);
    LOG_INFO("hook_execve:%p.",hook_execve);
    LOG_INFO("hook_posix_spawn:%p.",hook_posix_spawn);
    LOG_INFO("hook__mac_execve:%p.",hook__mac_execve);
    LOG_INFO("hook_read:%p.",hook_read);
    LOG_INFO("hook_write:%p.",hook_write);
    LOG_INFO("hook_delete:%p.",hook_delete);
    LOG_INFO("my_kmy_mach_msg_trapill:%p.",my_mach_msg_trap);
    LOG_INFO("my_mach_msg_overwrite_trap:%p.",my_mach_msg_overwrite_trap);
    
    syscall_table_hook[SYS_kill] = hook_kill;//37
    syscall_table_hook[SYS_open] = hook_open;//5
    syscall_table_hook[SYS_rename] = hook_rename;//128
    syscall_table_hook[SYS_copyfile] = hook_copyfile;//227
    syscall_table_hook[SYS_execve] = hook_execve;//59
    syscall_table_hook[SYS_posix_spawn] = hook_posix_spawn;//244
    syscall_table_hook[SYS___mac_execve] = hook__mac_execve;//380
    syscall_table_hook[SYS_read] = hook_read;//3
    syscall_table_hook[SYS_write] = hook_write;//4
    syscall_table_hook[SYS_delete] = hook_delete;//226
    
    disable_vm_protection();
    {
        g_mach_msg_trap = mach_table_hook_trap(MACH_MSG_TRAP, my_mach_msg_trap);
        g_mach_msg_overwrite_trap = mach_table_hook_trap(MACH_MSG_OVERWRITE_TRAP, my_mach_msg_overwrite_trap);
    }
    enable_vm_protection();

    
    //asm("int $3");
    
    //定义命令行指令
    sysctl_register_oid(&sysctl__debug_killhook);
    sysctl_register_oid(&sysctl__debug_killhook_hooked);
    sysctl_register_oid(&sysctl__debug_killhook_pid);
    sysctl_register_oid(&sysctl__debug_killhook_unhook);
    
/************************************************************************************************
 
 ctl_register
 
 ************************************************************************************************/
    strncpy(g_string_buf, Deflaut_String, strlen(Deflaut_String));
    strncpy(execve_string_buf, Deflaut_String, strlen(Deflaut_String));
    strncpy(file_string_buf, Deflaut_String, strlen(Deflaut_String));

    
    errno_t error;
    bzero(&ep_ctl, sizeof(ep_ctl));  // sets ctl_unit to 0
    ep_ctl.ctl_id = 0; /* OLD STYLE: ep_ctl.ctl_id = kEPCommID; */
    ep_ctl.ctl_unit = 0;
    strcpy(ep_ctl.ctl_name, BUNDLE_ID);
    ep_ctl.ctl_flags = CTL_FLAG_PRIVILEGED;// & CTL_FLAG_REG_ID_UNIT;
    ep_ctl.ctl_send = EPHandleWrite;
    ep_ctl.ctl_getopt = EPHandleGet;
    ep_ctl.ctl_setopt = EPHandleSet;
    ep_ctl.ctl_connect = EPHandleConnect;
    ep_ctl.ctl_disconnect = EPHandleDisconnect;
    error = ctl_register(&ep_ctl, &kctlref);
    if(!error)
        LOG_INFO("ctl_register success error:%p.",error);
    else{
        LOG_INFO("ctl_register filed error:%p.",error);
        return KERN_FAILURE;
    }
    
    g_register = TRUE;
//    @result 0 - Kernel control was registered.
//    EINVAL - The registration structure was not valid.
//    ENOMEM - There was insufficient memory.
//    EEXIST - A controller with that id/unit is already registered.
//    #define EINVAL          22      /* Invalid argument */
//    #define ENOMEM          12      /* Out of memory */
//    #define EEXIST          17      /* File exists */
    
    return KERN_SUCCESS;
}

kern_return_t test_stop(kmod_info_t *ki, void *d)
{
    // At this point a pointer to one of our hooked syscall may already be loaded by unix_syscall64
    // which leads to a race condition with our unload process (in-flight syscall may execute unloaded kext code)
    // This is a bad situation we can't really do anything about since we're not a part of syscall implementation path.
    // Disabling interrupts won't help since syscalls can be in flight on another core.
    // For now the best thing i can think of is to do unhook separetely, using a sysctl node and then unloading
    
    if (syscalls_hooked_all()) {
        LOG_ERROR("Please unhook syscalls before unloading (debug.killhook.unhook)");
        return KERN_ABORTED;
    }
    
    if(g_register){
        LOG_INFO("ctl_unregister");
        ctl_deregister(kctlref);
    }
    
    sysctl_unregister_oid(&sysctl__debug_killhook);
    sysctl_unregister_oid(&sysctl__debug_killhook_hooked);
    sysctl_unregister_oid(&sysctl__debug_killhook_pid);
    sysctl_unregister_oid(&sysctl__debug_killhook_unhook);
    
    lck_mtx_free(g_task_lock, g_lock_group);
    lck_grp_free(g_lock_group);
    
    OSMalloc_Tagfree(g_tag);
    
    LOG_INFO("moonAgirl[test_stop] done.");
    
    return KERN_SUCCESS;
}
