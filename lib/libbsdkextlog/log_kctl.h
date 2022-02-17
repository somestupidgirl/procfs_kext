/*
 * Created 190414 lynnl
 *
 * Implements kernel control socket functionalities
 */

#ifndef LOG_KCTL_H
#define LOG_KCTL_H

#include <sys/systm.h>

#include "kextlog.h"

kern_return_t log_kctl_register(void);
kern_return_t log_kctl_deregister(void);

void log_printf(uint32_t, const char *, ...) __printflike(2, 3);

#define log_trace(fmt, ...) \
    log_printf(KEXTLOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)

#ifdef DEBUG
#define log_debug(fmt, ...) \
    log_printf(KEXTLOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
#define log_debug(fmt, ...) UNUSED(fmt, ##__VA_ARGS__)
#endif

#define log_info(fmt, ...) \
    log_printf(KEXTLOG_LEVEL_INFO, fmt, ##__VA_ARGS__)

#define log_warning(fmt, ...) \
    log_printf(KEXTLOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)

#define log_error(fmt, ...) \
    log_printf(KEXTLOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#endif /* LOG_KCTL_H */

