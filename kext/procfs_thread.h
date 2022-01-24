#ifndef procfs_thread_h
#define procfs_thread_h

#include <stddef.h>
#include <os/refcnt.h>
#include <sys/cdefs.h>

#define THREAD_SELF_PORT_COUNT 3

/* Type used for thread->sched_mode and saved_mode */
typedef enum {
    TH_MODE_NONE = 0,                                       /* unassigned, usually for saved_mode only */
    TH_MODE_REALTIME,                                       /* time constraints supplied */
    TH_MODE_FIXED,                                          /* use fixed priorities, no decay */
    TH_MODE_TIMESHARE,                                      /* use timesharing algorithm */
} sched_mode_t;

struct machine_thread {
    uint64_t                 cthread_self;
};

typedef struct timer        *timer_data_t, *timer_t;

struct thread {
    uint16_t                 options;
    vm_offset_t              kernel_stack;
    int                      state;
    sched_mode_t             sched_mode;
    uint32_t
        active:1,           /* Thread is active and has not been terminated */
        ipc_active:1,       /* IPC with the thread ports is allowed */
        started:1,          /* Thread has been started after creation */
        static_param:1,     /* Disallow policy parameter changes */
        inspection:1,       /* TRUE when task is being inspected by crash reporter */
        policy_reset:1,     /* Disallow policy parameter changes on terminating threads */
        suspend_parked:1,   /* thread parked in thread_suspended */
        corpse_dup:1,       /* TRUE when thread is an inactive duplicate in a corpse */
    :0;
    lck_mtx_t               *mutex;
    os_refcnt_t              ref_count;
    timer_data_t             runnable_timer;
    uint32_t                 sched_flags;
    int16_t                  sched_pri;
    int16_t                  base_pri;
    int16_t                  max_priority;
    int                      precise_user_kernel_time;
    int16_t                  user_stop_count;
    queue_chain_t            task_threads;
    timer_data_t             system_timer;
    timer_data_t             user_timer;
    struct task             *task;
    struct ipc_port         *ith_thread_ports[THREAD_SELF_PORT_COUNT];
    void                    *uthread;
    uint64_t                 thread_id;
    struct machine_thread    machine;
};

typedef struct thread       *thread_t, *thread_act_t, *thread_inspect_t, *thread_read_t;

struct task {
    boolean_t                active;
    bool                     ipc_active;
    queue_head_t             threads;
    int                      thread_count;
    lck_mtx_t               *lock;
    void                    *bsd_info;
    uint64_t                 dispatchqueue_offset;
};

#define MINPRI                          0
#define MINPRI_USER                     MINPRI                          /* 0 */
#define DEPRESSPRI                      (MINPRI)                        /* depress priority */

#define TH_SFLAG_DEPRESS                0x0040                          /* normal depress yield */
#define TH_SFLAG_POLLDEPRESS            0x0080                          /* polled depress yield */
#define TH_SFLAG_DEPRESSED_MASK         (TH_SFLAG_DEPRESS | TH_SFLAG_POLLDEPRESS)

/* capability strictly _DECREASING_.
 * not ordered the other way around because we want TASK_FLAVOR_CONTROL
 * to be closest to the itk_lock. see task.h.
 */
typedef unsigned int                    mach_task_flavor_t;
#define TASK_FLAVOR_CONTROL             0                               /* a task_t */
#define TASK_FLAVOR_READ                1                               /* a task_read_t */
#define TASK_FLAVOR_INSPECT             2                               /* a task_inspect_t */
#define TASK_FLAVOR_NAME                3                               /* a task_name_t */

/* capability strictly _DECREASING_ */
typedef unsigned int                    mach_thread_flavor_t;
#define THREAD_FLAVOR_CONTROL           0       /* a thread_t */
#define THREAD_FLAVOR_READ              1       /* a thread_read_t */
#define THREAD_FLAVOR_INSPECT           2       /* a thread_inspect_t */

/*
 *  Thread states [bits or'ed]
 */
#define TH_WAIT                 0x01            /* queued for waiting */
#define TH_SUSP                 0x02            /* stopped or requested to stop */
#define TH_RUN                  0x04            /* running or on runq */
#define TH_UNINT                0x08            /* waiting uninteruptibly */
#define TH_TERMINATE            0x10            /* halted at termination */
#define TH_TERMINATE2           0x20            /* added to termination queue */
#define TH_WAIT_REPORT          0x40            /* the wait is using the sched_call,
                                            * only set if TH_WAIT is also set */
#define TH_IDLE                 0x80            /* idling processor */

#define TH_OPT_IDLE_THREAD              0x0080
#define TH_OPT_GLOBAL_FORCED_IDLE       0x0100

typedef unsigned                        spl_t;
#define splsched()                      (spl_t) ml_set_interrupts_enabled(FALSE)
#define splx(x)                         (void) ml_set_interrupts_enabled(x)

/* 
 * For DEV & REL kernels, use a static dispatch table instead of 
 * using the indirect function table.
 */
//FIXME:
//extern const struct sched_dispatch_table sched_multiq_dispatch;
#define SCHED(f)                        //(sched_multiq_dispatch.f)

#define thread_reference_internal(thread)       \
                    os_ref_retain(&(thread)->ref_count);

#define __IGNORE_WCASTALIGN(x)          _Pragma("clang diagnostic push")                     \
                                        _Pragma("clang diagnostic ignored \"-Wcast-align\"") \
                                        x;                                                   \
                                        _Pragma("clang diagnostic pop")

kern_return_t thread_info_internal(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info_out, mach_msg_type_number_t *thread_info_count);
kern_return_t thread_info(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);
kern_return_t task_threads(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);

#endif /* procfs_thread_h */
