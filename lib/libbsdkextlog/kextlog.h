/*
 * Created 190414 lynnl
 *
 * Exported header used both in kext and user space log daemon
 */

#ifndef KEXTLOG_H
#define KEXTLOG_H

#include <stdint.h>
#include <sys/socket.h>

#define KEXTLOG_KCTL_NAME           "com.stupid.filesystems.procfs.kctl"
#define KEXTLOG_KCTL_SOCKTYPE       SOCK_DGRAM

#define KEXTLOG_LEVEL_TRACE         0
#define KEXTLOG_LEVEL_DEBUG         1
#define KEXTLOG_LEVEL_INFO          2
#define KEXTLOG_LEVEL_WARNING       3
#define KEXTLOG_LEVEL_ERROR         4

/*
 * Indicate direct-previous messages dropped due to failure
 */
#define KEXTLOG_FLAG_MSG_DROPPED    0x1
#define KEXTLOG_FLAG_MSG_TRUNCATED  0x2

#define _KEXTLOG_PADDING_MAGIC      0x65636166  /* Little-endian 'face' */

struct kextlog_msghdr {
    int32_t pid;
    uint64_t tid;

    uint64_t timestamp;     /* always be mach_absolute_time() */
    uint32_t level;
    uint32_t flags;

    uint32_t size;          /* Size of message buffer */
    uint32_t _padding;

    /*
     * Zero size must be given to silence
     *  -Wgnu-variable-sized-type-not-at-end for nested structs
     */
    char buffer[0];
} __attribute__ ((aligned (8)));

#endif /* KEXTLOG_H */

