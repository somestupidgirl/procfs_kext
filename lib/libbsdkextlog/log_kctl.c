/*
 * Created 190414 lynnl
 */

#include <stdint.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/kern_control.h>
#include <libkern/OSAtomic.h>
#include <mach/mach_time.h>
#include <sys/vm.h>

#include "log_kctl.h"
#include "utils.h"
#include "kextlog.h"
#include "log_sysctl.h"

static errno_t log_kctl_connect( kern_ctl_ref, struct sockaddr_ctl *, void **);
static errno_t log_kctl_disconnect(kern_ctl_ref, u_int32_t, void *);

#define SOCK2FLAG(t)        ((t) == SOCK_STREAM ? CTL_FLAG_REG_SOCK_STREAM : 0)

/*
 * NOTE:
 *  xnu/bsd/kern/kern_control.c
 *      #define CTL_SENDSIZE (2 * 1024)     //  data into kernel
 *      #define CTL_RECVSIZE (8 * 1024)     //  data to user space
 *  above denoted default send/recv buffer size
 *
 * see: xnu/bsd/kern/kern_control.c#ctl_getenqueuespace
 */
static struct kern_ctl_reg kctlreg = {
    KEXTLOG_KCTL_NAME,                  /* ctl_name */
    0,                                  /* ctl_id */
    0,                                  /* ctl_unit */
    SOCK2FLAG(KEXTLOG_KCTL_SOCKTYPE),   /* ctl_flags */
    0,                                  /* ctl_sendsize */
    0,                                  /* ctl_recvsize */
    log_kctl_connect,                   /* ctl_connect */
    log_kctl_disconnect,                /* ctl_disconnect */
    NULL,                               /* ctl_send */
    NULL,                               /* ctl_setopt */
    NULL,                               /* ctl_getopt */
};

static kern_ctl_ref kctlref = NULL;
static volatile u_int32_t kctlunit = 0;     /* Active unit is positive */

/*
 * XXX: kctl can be connected in the interim of kext loading
 * You may happen to take further logic to
 *  prevent client from connecting in such case
 */
static errno_t log_kctl_connect(
        kern_ctl_ref ref,
        struct sockaddr_ctl *sac,
        void **unitinfo)
{
    errno_t e = 0;

    BUILD_BUG_ON(sizeof(u_int32_t) != sizeof(UInt32));

    kassert(ref == kctlref);

    if (OSCompareAndSwap(0, sac->sc_unit, (UInt32 *) &kctlunit)) {
        kassert_nonnull(unitinfo);
        *unitinfo = NULL;
        LOG_DBG("Log kctl connected  unit: %u", sac->sc_unit);
    } else {
        e = EISCONN;
        LOG_WARN("Log kctl already connected  skip");
    }

    return e;
}

static errno_t log_kctl_disconnect(
        kern_ctl_ref ref,
        u_int32_t unit,
        void *unitinfo)
{
    UNUSED(ref);
    if (OSCompareAndSwap(unit, 0, &kctlunit)) {
        kassert(unitinfo == NULL);
        LOG_DBG("Log kctl client disconnected  unit: %u", unit);
    } else {
        /* Refused clients */
    }
    return 0;
}

kern_return_t log_kctl_register(void)
{
    errno_t e = ctl_register(&kctlreg, &kctlref);
    if (e == 0) {
        LOG_DBG("kctl %s registered  ref: %p", KEXTLOG_KCTL_NAME, kctlref);
    } else {
        LOG_ERR("ctl_register() fail  errno: %d", e);
    }
    return e ? KERN_FAILURE : KERN_SUCCESS;
}

kern_return_t log_kctl_deregister(void)
{
    errno_t e = 0;
    /* ctl_deregister(NULL) returns EINVAL */
    e = ctl_deregister(kctlref);
    if (e == 0) {
        LOG_DBG("kctl %s deregistered  ref: %p", KEXTLOG_KCTL_NAME, kctlref);
    } else {
        LOG_ERR("ctl_deregister() fail  ref: %p errno: %d", kctlref, e);
    }
    return e ? KERN_FAILURE : KERN_SUCCESS;
}

#define MSG_BUFSZ       4096

/**
 * Print message to system message buffer(last resort)
 * The message may truncated if it's far too large
 */
static inline void log_syslog(uint32_t level, const char *fmt, va_list ap)
{
    static char buf[MSG_BUFSZ];
    static volatile uint32_t spin_lock = 0;
    Boolean ok;

    kassert_nonnull(fmt);

    /* vsnprintf, printf should fast  thus spin lock do no hurts? */
    while (!OSCompareAndSwap(0, 1, &spin_lock)) continue;

    (void) vsnprintf(buf, MSG_BUFSZ, fmt, ap);
    switch (level) {
    case KEXTLOG_LEVEL_TRACE:
        LOG_TRACE("%s", buf);
        break;
    case KEXTLOG_LEVEL_DEBUG:
        LOG_DBG("%s", buf);
        break;
    case KEXTLOG_LEVEL_INFO:
        LOG("%s", buf);
        break;
    case KEXTLOG_LEVEL_WARNING:
        LOG_WARN("%s", buf);
        break;
    case KEXTLOG_LEVEL_ERROR:
        LOG_ERR("%s", buf);
        break;
    default:
        panicf("unswitched log level %u", level);
    }

    ok = OSCompareAndSwap(1, 0, &spin_lock);
    kassertf(ok, "OSCompareAndSwap() 1 to 0 fail  val: %#x", spin_lock);
}

static int enqueue_log(struct kextlog_msghdr *msg, size_t len)
{
    static uint8_t last_dropped = 0;
    static volatile uint32_t spin_lock = 0;

    kern_ctl_ref ref = kctlref;
    u_int32_t unit = kctlunit;
    errno_t e;
    Boolean ok;

    kassert_nonnull(msg);
    kassertf(sizeof(*msg) + msg->size == len, "Message size mismatch  %zu vs %zu", sizeof(*msg) + msg->size, len);

    /* TODO: use mutex instead of busy spin lock? */
    while (!OSCompareAndSwap(0, 1, &spin_lock)) continue;

    if (unit == 0) {
        e = ENOTCONN;
        goto out_unlock;
    }

    if (last_dropped) {
        last_dropped = 0;
        msg->flags |= KEXTLOG_FLAG_MSG_DROPPED;
    }

    /* Message buffer's `\0' will also push into user space */
    e = ctl_enqueuedata(ref, unit, msg, len, 0);

out_unlock:
    if (e != 0) last_dropped = 1;
    ok = OSCompareAndSwap(1, 0, &spin_lock);
    kassertf(ok, "OSCompareAndSwap() 1 to 0 fail  val: %#x", spin_lock);

    if (e != 0) {
        LOG_ERR("ctl_enqueuedata() fail  ref: %p unit: %u len: %zu errno: %d", ref, unit, len, e);
    }

    return e;
}

#define KEXTLOG_STACKMSG_SIZE       128

struct kextlog_stackmsg {
    struct kextlog_msghdr hdr;
    char buffer[KEXTLOG_STACKMSG_SIZE];
};

void log_printf(uint32_t level, const char *fmt, ...)
{
    struct kextlog_stackmsg msg;
    struct kextlog_msghdr *msgp;
    int len;
    int len2;
    va_list ap;
    uint32_t msgsz;
    uint32_t flags = 0;

    kassertf(level >= KEXTLOG_LEVEL_TRACE && level <= KEXTLOG_LEVEL_ERROR, "Bad log level %u", level);
    kassert_nonnull(fmt);

    msgp = (struct kextlog_msghdr *) &msg;

    /* Push message to syslog if log kctl not yet ready */
    if (kctlunit == 0) goto out_sysmbuf;

    va_start(ap, fmt);
    /*
     * [sic vsnprintf(3)]
     * vsnprintf() return the number of characters that would have been printed
     *  if the size were unlimited(not including the final `\0')
     *
     * return value of vsnprintf() always non-negative
     */
    len = vsnprintf(msg.buffer, sizeof(msg.buffer), fmt, ap);
    va_end(ap);

    /* msgsz = sizeof(*msgp) + len + 1; */
    if (__builtin_uadd_overflow(sizeof(*msgp), len, &msgsz) ||
        /* Includes log message trailing `\0' */
        __builtin_uadd_overflow(msgsz, 1, &msgsz)) {
        /* Should never happen */
        LOG_ERR("log_printf() message size overflow  level: %u fmt: %s msgsz: %u", level, fmt, msgsz);
        goto out_overflow;
    }

    if (len >= (int) sizeof(msg.buffer)) {
        msgp = (struct kextlog_msghdr *) util_malloc0(msgsz, M_WAITOK | M_NULL);
        if (msgp != NULL) {
            va_start(ap, fmt);
            len2 = vsnprintf(msgp->buffer, len + 1, fmt, ap);
            va_end(ap);

            kassert_eq(len, len2, "%d", "%d");
            (void) OSIncrementAtomic64((SInt64 *) &log_stat.heapmsg);
        } else {
            (void) OSIncrementAtomic64((SInt64 *) &log_stat.oom);

            msgp = (struct kextlog_msghdr *) &msg;
out_overflow:
            msgsz = sizeof(msg);
            len = sizeof(msg.buffer) - 1;
            flags |= KEXTLOG_FLAG_MSG_TRUNCATED;
        }
    } else {
        (void) OSIncrementAtomic64((SInt64 *) &log_stat.stackmsg);
    }

    msgp->pid = proc_pid(current_proc());
    msgp->tid = thread_tid(current_thread());
    msgp->timestamp = mach_absolute_time();
    msgp->level = level;
    msgp->flags = flags;
    msgp->size = len + 1;
    msgp->_padding = _KEXTLOG_PADDING_MAGIC;

    if (enqueue_log(msgp, msgsz) != 0) {
        (void) OSIncrementAtomic64((SInt64 *) &log_stat.enqueue_failure);

out_sysmbuf:
        (void) OSIncrementAtomic64((SInt64 *) &log_stat.syslog);

        va_start(ap, fmt);
        log_syslog(level, fmt, ap);
        va_end(ap);
    }

    if (msgp != (struct kextlog_msghdr *) &msg) {
        /* util_mfree(NULL, type) do nop */
        util_mfree(msgp);
    }
}

