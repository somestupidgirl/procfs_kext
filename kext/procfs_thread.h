#ifndef procfs_thread_h
#define procfs_thread_h

#include <os/refcnt.h>

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

struct thread {
    sched_mode_t             sched_mode;
    lck_mtx_t               *mutex;
    os_refcnt_t              ref_count;
    uint32_t                 sched_flags;
    int16_t                  sched_pri;
    int16_t                  base_pri;
    int16_t                  max_priority;
    queue_chain_t            task_threads;
    struct task             *task;
    void                    *uthread;
    uint64_t                 thread_id;
    struct machine_thread    machine;
};

struct task {
    boolean_t                active;
    queue_head_t             threads;
    int                      thread_count;
    lck_mtx_t               *lock;
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

#define __IGNORE_WCASTALIGN(x)          _Pragma("clang diagnostic push")                     \
                                        _Pragma("clang diagnostic ignored \"-Wcast-align\"") \
                                        x;                                                   \
                                        _Pragma("clang diagnostic pop")

kern_return_t procfs_thread_info(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);
kern_return_t procfs_task_threads(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);

#endif /* procfs_thread_h */
