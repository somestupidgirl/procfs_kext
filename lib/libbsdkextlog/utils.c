/*
 * Created 190415 lynnl
 */

#include <sys/time.h>
#include <sys/proc.h>           /* msleep() */
#include <libkern/OSAtomic.h>
#include <sys/malloc.h>

#include "utils.h"

#define KCB_OPT_GET         0
#define KCB_OPT_PUT         1
#define KCB_OPT_READ        2
#define KCB_OPT_INVALIDATE  3

/**
 * kcb stands for kernel callbacks  a global refcnt used in kext
 * @return      -1(actually negative value) if kcb invalidated
 */
static inline int kcb(int opt)
{
    static volatile SInt i = 0;
    static struct timespec ts = {0, 1e+6};  /* 100ms */
    SInt rd;

    BUILD_BUG_ON(sizeof(SInt) != sizeof(int));

    switch (opt) {
    case KCB_OPT_GET:
        do {
            if ((rd = i) < 0) break;
        } while (!OSCompareAndSwap(rd, rd + 1, &i));
        break;

    case KCB_OPT_PUT:
        rd = OSDecrementAtomic(&i);
        kassertf(rd > 0, "non-positive counter %d", i);
        break;

    case KCB_OPT_INVALIDATE:
        do {
            while (i > 0) (void) msleep((void *) &i, NULL, PWAIT, NULL, &ts);
        } while (i >= 0 && !OSCompareAndSwap(0, (UInt32) -1, &i));
        /* Fall through */

    case KCB_OPT_READ:
        rd = i;
        break;

    default:
        panicf("invalid option  opt: %d i: %d", opt, i);
    }

    return rd;
}

/**
 * Increase refcnt of activated kext callbacks
 * @return      -1 if failed to get
 *              refcnt before get o.w.
 */
int kcb_get(void)
{
    return kcb(KCB_OPT_GET);
}

/**
 * Decrease refcnt of activated kext callbacks
 * @return      refcnt before put o.w.
 */
int kcb_put(void)
{
    return kcb(KCB_OPT_PUT);
}

/**
 * Read refcnt of activated kext callbacks(rarely used)
 * @return      a snapshot of refcnt
 */
int kcb_read(void)
{
    return kcb(KCB_OPT_READ);
}

/**
 * Invalidate kcb counter
 * Will block until all threads stopped and counter invalidated
 */
void kcb_invalidate(void)
{
    (void) kcb(KCB_OPT_INVALIDATE);
}

static void util_mstat(int opt)
{
    static volatile SInt64 cnt = 0;
    switch (opt) {
    case 0:
        if (OSDecrementAtomic64(&cnt) > 0) return;
        break;
    case 1:
        if (OSIncrementAtomic64(&cnt) >= 0) return;
        break;
    case 2:
        if (cnt == 0) return;
        break;
    }
    panicf("FIXME: potential memleak  opt: %d cnt: %lld", opt, cnt);
}

/* Zero size allocation will return a NULL */
void * __nullable util_malloc0(size_t size, int flags)
{
    /* _MALLOC `type' parameter is a joke */
    void *addr = _MALLOC(size, M_TEMP, flags);
    if (likely(addr != NULL)) util_mstat(1);
    return addr;
}

void * __nullable util_malloc(size_t size)
{
    return util_malloc0(size, M_NOWAIT);
}

void util_mfree(void * __nullable addr)
{
    if (addr != NULL) {
        _FREE(addr, M_TEMP);
        util_mstat(0);
    }
}

/* XXX: call when all memory freed */
void util_massert(void)
{
    util_mstat(2);
}

