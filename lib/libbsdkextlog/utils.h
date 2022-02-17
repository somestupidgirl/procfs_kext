/*
 * Created 190414 lynnl
 *
 * Utility macros & functions(if any)
 */

#ifndef UTILS_H
#define UTILS_H

#include <libkern/libkern.h>    /* printf() */
#include <kern/debug.h>

#ifndef __kext_makefile__
#define KEXTNAME_S          "bsd_kext_log"
#endif

/*
 * Used to indicate unused function parameters
 * see: <sys/cdefs.h>#__unused
 */
#define UNUSED(e, ...)      (void) ((void) (e), ##__VA_ARGS__)

#define ARRAY_SIZE(a)       (sizeof(a) / sizeof(*a))

/**
 * Compile-time assurance  see: linux/arch/x86/boot/boot.h
 * Will fail build if condition yield true
 */
#ifdef DEBUG
#define BUILD_BUG_ON(cond)      UNUSED(sizeof(char[-!!(cond)]))
#else
#define BUILD_BUG_ON(cond)      UNUSED(cond)
#endif

/* XXX: ONLY quote those deprecated functions */
#define SUPPRESS_WARN_DEPRECATED_DECL_BEGIN     \
    _Pragma("clang diagnostic push")            \
    _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")

#define SUPPRESS_WARN_DEPRECATED_DECL_END       \
    _Pragma("clang diagnostic pop")

/**
 * os_log() is only available on macOS 10.12 or newer
 *  thus os_log do have compatibility issue  use printf instead
 *
 * XNU kernel version of printf() don't recognize some rarely used specifiers
 *  like h, i, j, t  use unrecognized spcifier may raise kernel panic
 *
 * Feel free to print NULL as %s  it checked explicitly by kernel-printf
 *
 * see: xnu/osfmk/kern/printf.c#printf
 */
#define LOG(fmt, ...)        printf(KEXTNAME_S ": " fmt "\n", ##__VA_ARGS__)

#define LOG_WARN(fmt, ...)   LOG("[WARN] " fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)    LOG("[ERR] " fmt, ##__VA_ARGS__)
#define LOG_BUG(fmt, ...)    LOG("[BUG] " fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...)  LOG("[TRACE] " fmt, ##__VA_ARGS__)
#define LOG_OFF(fmt, ...)    UNUSED(fmt, ##__VA_ARGS__)
#ifdef DEBUG
#define LOG_DBG(fmt, ...)    LOG("[DBG] " fmt, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...)    LOG_OFF(fmt, ##__VA_ARGS__)
#endif

#define panicf(fmt, ...) ({                                     \
    panic("\n" fmt "\n%s@%s()#%d\n\n",                          \
        ##__VA_ARGS__, __BASE_FILE__, __func__, __LINE__);      \
    __builtin_unreachable();                                    \
})

#ifdef DEBUG
/*
 * NOTE: Do NOT use any multi-nary conditional/logical operator inside assertion
 *       like operators && || ?:  it's extremely EVIL
 *       Separate them  each statement per line
 */
#define kassert(ex) (ex) ? (void) 0 : panicf("Assert `%s' failed", #ex)

/**
 * @ex      the expression
 * @fmt     panic message format
 *
 * Example: kassertf(sz > 0, "Why size %zd non-positive?", sz);
 */
#define kassertf(ex, fmt, ...) \
    (ex) ? (void) 0 : panicf("Assert `%s' failed: " fmt, #ex, ##__VA_ARGS__)
#else
#define kassert(ex) (ex) ? (void) 0 : LOG_BUG("Assert `%s' failed", #ex)

#define kassertf(ex, fmt, ...) \
    (ex) ? (void) 0 : LOG_BUG("Assert `%s' failed: " fmt, #ex, ##__VA_ARGS__)
#endif

#define kassert_nonnull(ptr)    kassert(ptr != NULL)
#define kassert_null(ptr)       kassert(ptr == NULL)

#define __kassert_cmp(v1, v2, f1, f2, op)   \
    kassertf((v1) op (v2), "left: " f1 " right: " f2, (v1), (v2))

#define kassert_eq(v1, v2, f1, f2)  __kassert_cmp(v1, v2, f1, f2, ==)
#define kassert_ne(v1, v2, f1, f2)  __kassert_cmp(v1, v2, f1, f2, !=)
#define kassert_le(v1, v2, f1, f2)  __kassert_cmp(v1, v2, f1, f2, <=)
#define kassert_ge(v1, v2, f1, f2)  __kassert_cmp(v1, v2, f1, f2, >=)
#define kassert_lt(v1, v2, f1, f2)  __kassert_cmp(v1, v2, f1, f2, <)
#define kassert_gt(v1, v2, f1, f2)  __kassert_cmp(v1, v2, f1, f2, >)

/**
 * Branch predictions
 * see: linux/include/linux/compiler.h
 */
#define likely(x)               __builtin_expect(!!(x), 1)
#define unlikely(x)             __builtin_expect(!!(x), 0)

int kcb_get(void);
int kcb_put(void);
int kcb_read(void);
void kcb_invalidate(void);

void * __nullable util_malloc0(size_t, int);
void * __nullable util_malloc(size_t);
void util_mfree(void * __nullable);
void util_massert(void);

#endif /* UTILS_H */

