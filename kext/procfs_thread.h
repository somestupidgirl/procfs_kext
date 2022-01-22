#ifndef procfs_thread_h
#define procfs_thread_h

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
