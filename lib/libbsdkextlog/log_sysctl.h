/*
 * Created 190423 lynnl
 */

#ifndef LOG_SYSCTL_H
#define LOG_SYSCTL_H

struct kextlog_statistics {
    volatile uint64_t syslog;
    volatile uint64_t heapmsg;
    volatile uint64_t stackmsg;
    volatile uint64_t oom;
    volatile uint64_t enqueue_failure;
};

extern struct kextlog_statistics log_stat;

void log_sysctl_register(void);
void log_sysctl_deregister(void);

#endif /* LOG_SYSCTL_H */

