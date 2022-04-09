/**
 * Created 180826 lynnl
 */

#include <sys/systm.h>
#include <libkern/OSAtomic.h>
#include <mach-o/loader.h>
#include <sys/vnode.h>

#include "libkext.h"

static void
libkext_mstat(int opt)
{
    static volatile SInt64 cnt = 0;
    switch (opt) {
    case 0:
        if (OSDecrementAtomic64(&cnt) > 0) return;
        break;
    case 1:
        if (OSIncrementAtomic64(&cnt) >= 0) return;
        break;
    default:
        if (cnt == 0) return;
        break;
    }
#ifdef DEBUG
    panicf("FIXME: potential memleak  opt: %d cnt: %lld", opt, cnt);
    __builtin_unreachable();
#else
    LOG_BUG("FIXME: potential memleak  opt: %d cnt: %lld", opt, cnt);
#endif
}

/* Zero size allocation will return a NULL */
void *
libkext_malloc(size_t size, int flags)
{
    /* _MALLOC `type' parameter is a joke */
    void *addr = _MALLOC(size, M_TEMP, flags);
    if (likely(addr != NULL)) libkext_mstat(1);
    return addr;
}

/**
 * Poor replica of _REALLOC() in XNU
 *
 * /System/Library/Frameworks/Kernel.framework/Resources/SupportedKPIs-all-archs.txt
 * Listed all supported KPI  as it revealed
 *  _MALLOC and _FREE both supported  where _REALLOC not exported by Apple
 *
 * @param addr0     Address needed to reallocation
 * @param sz0       Original size of the buffer
 * @param sz1       New size
 * @param flags     Flags to malloc
 * @return          Return NULL on fail  O.w. new allocated buffer
 *
 * NOTE:
 *  You should generally avoid allocate zero-length(new buffer size)
 *  the behaviour is implementation-defined(_MALLOC return NULL in such case)
 *
 * See:
 *  xnu/bsd/kern/kern_malloc.c@_REALLOC
 *  wiki.sei.cmu.edu/confluence/display/c/MEM04-C.+Beware+of+zero-length+allocations
 */
static void *
libkext_realloc2(void *addr0, size_t sz0, size_t sz1, int flags)
{
    void *addr1;

    /*
     * [sic] _REALLOC(NULL, ...) is equivalent to _MALLOC(...)
     * XXX  in such case, we require its original size must be zero
     */
    if (addr0 == NULL) {
        kassert(sz0 == 0);
        addr1 = _MALLOC(sz1, M_TEMP, flags);
        goto out_exit;
    }

    if (unlikely(sz1 == sz0)) {
        addr1 = addr0;
        goto out_exit;
    }

    addr1 = _MALLOC(sz1, M_TEMP, flags);
    if (unlikely(addr1 == NULL))
        goto out_exit;

    memcpy(addr1, addr0, MIN(sz0, sz1));
    _FREE(addr0, M_TEMP);

out_exit:
    return addr1;
}

void *
libkext_realloc(void *addr0, size_t sz0, size_t sz1, int flags)
{
    void *addr1 = libkext_realloc2(addr0, sz0, sz1, flags);
    /*
     * If addr0 is nonnull yet addr1 null  the reference shouldn't change
     *  since addr0 won't be free in such case
     */
    if (!addr0 && addr1) libkext_mstat(1);
    return addr1;
}

void
libkext_mfree(void *addr)
{
    if (addr != NULL) libkext_mstat(0);
    _FREE(addr, M_TEMP);
}

/* XXX: call when all memory freed */
void
libkext_massert(void)
{
    libkext_mstat(2);
}

#define KCB_OPT_GET         0
#define KCB_OPT_PUT         1
#define KCB_OPT_READ        2
#define KCB_OPT_INVALIDATE  3

/**
 * kcb stands for kernel callbacks  a global refcnt used in kext
 * @return      -1(actually negative value) if kcb invalidated
 */
static inline int
kcb(int opt)
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
            rd = i;
            kassertf(rd >= 0, "invalidate kcb more than once?!  i: %d", rd);
            while (i > 0) (void) msleep((void *) &i, NULL, PWAIT, NULL, &ts);
        } while (!OSCompareAndSwap(0, (UInt32) -1, &i));
        /* Fall through */

    case KCB_OPT_READ:
        rd = i;
        break;

    default:
        panicf("invalid option  opt: %d i: %d", opt, i);
        __builtin_unreachable();
    }

    return rd;
}

/**
 * Increase refcnt of activated kext callbacks
 * @return      -1 if failed to get
 *              refcnt before get o.w.
 */
int
libkext_get_kcb(void)
{
    return kcb(KCB_OPT_GET);
}

/**
 * Decrease refcnt of activated kext callbacks
 * @return      refcnt before put o.w.
 */
int
libkext_put_kcb(void)
{
    return kcb(KCB_OPT_PUT);
}

/**
 * Read refcnt of activated kext callbacks(rarely used)
 * @return      a snapshot of refcnt
 */
int
libkext_read_kcb(void)
{
    return kcb(KCB_OPT_READ);
}

/**
 * Invalidate kcb counter
 * You should call this function only once after all spawn threads unattached
 * Will block until all threads stopped and counter invalidated
 */
void
libkext_invalidate_kcb(void)
{
    (void) kcb(KCB_OPT_INVALIDATE);
}

#define UUID_STR_BUFSZ      sizeof(uuid_string_t)

/**
 * Extract UUID load command from a Mach-O address
 *
 * @addr    Mach-O starting address
 * @output  UUID string output
 * @return  0 if success  errno o.w.
 */
int
libkext_vma_uuid(vm_address_t addr, uuid_string_t output)
{
    kassert_nonnull(addr);
    kassert_nonnull(output);

    int e = 0;
    uint8_t *p = (void *) addr;
    struct mach_header *h = (struct mach_header *) addr;
    struct load_command *lc;
    uint32_t i;
    uint8_t *u;

    kassert_nonnull(addr);

    if (h->magic == MH_MAGIC || h->magic == MH_CIGAM) {
        p += sizeof(struct mach_header);
    } else if (h->magic == MH_MAGIC_64 || h->magic == MH_CIGAM_64) {
        p += sizeof(struct mach_header_64);
    } else {
        e = EBADMACHO;
        goto out_bad;
    }

    for (i = 0; i < h->ncmds; i++, p += lc->cmdsize) {
        lc = (struct load_command *) p;
        if (lc->cmd == LC_UUID) {
            u = p + sizeof(*lc);

            (void) snprintf(output, UUID_STR_BUFSZ,
                    "%02x%02x%02x%02x-%02x%02x-%02x%02x-"
                    "%02x%02x-%02x%02x%02x%02x%02x%02x",
                    u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7],
                    u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
            goto out_bad;
        }
    }

    e = ENOENT;

out_bad:
    return e;
}

/**
 * Format UUID data into human readable string
 * @u       UUID data
 * @output  UUID string output
 */
void
libkext_format_uuid_string(const uuid_t u, uuid_string_t output)
{
    kassert_nonnull(u);
    kassert_nonnull(output);

    (void) snprintf(output, sizeof(uuid_string_t),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7],
            u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
}

/**
 * Read a file from local volume(won't follow symlink)
 * @path        file path
 * @buff        read buffer
 * @len         length of read buffer
 * @off         read offset
 * @read        (OUT) bytes read(set if success)
 * @return      0 if success  errno o.w.
 */
int
libkext_file_read(const char *path, unsigned char *buff,
                    size_t len, off_t off, size_t * __nullable read)
{
    errno_t e;
    int flag = VNODE_LOOKUP_NOFOLLOW | VNODE_LOOKUP_NOCROSSMOUNT;
    vnode_t vp;
    vfs_context_t ctx;
    uio_t auio;

    kassert_nonnull(path);
    kassert_nonnull(buff);

    ctx = vfs_context_create(NULL);
    if (unlikely(ctx == NULL)) {
        e = ENOMEM;
        goto out_oom;
    }

    auio = uio_create(1, off, UIO_SYSSPACE, UIO_READ);
    if (unlikely(auio == NULL)) {
        e = ENOMEM;
        goto out_ctx;
    }

    e = uio_addiov(auio, (user_addr_t) buff, len);
    if (unlikely(e)) goto out_uio;

    e = vnode_lookup(path, flag, &vp, ctx);
    if (unlikely(e)) goto out_uio;

    e = VNOP_READ(vp, auio, IO_NOAUTH, ctx);
    if (unlikely(e)) goto out_put;

    if (read != NULL) *read = len - uio_resid(auio);

out_put:
    vnode_put(vp);
out_uio:
    uio_free(auio);
out_ctx:
    vfs_context_rele(ctx);
out_oom:
    return e;
}
