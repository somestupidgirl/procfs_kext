/*
 * klookup.c
 *
 * Resolve private kernel symbols staged by the procfs_ksyms userspace helper.
 *
 * SPDX-License-Identifier: 0BSD
 *
 * Copyright (c) 2021 Syncretic
 * Copyright (c) 2024-2026 Sunneva N. Mariu
 *
 * The running arm64 kernel jettisons its __LINKEDIT after boot, and the only
 * on-disk symbol table for the running build lives in the booted kernel
 * collection, which is LZFSE-compressed (and lzfse is not linkable in-kernel).
 * So the heavy lifting - decompress the kernelcache, read its symtab, and record
 * the private symbols' link-time addresses - is done in userspace by
 * tools/procfs_ksyms, which writes them to a small staged file. Here we just
 * read that file, recover the running KASLR slide from `version`
 * (slide = &version - staged("_version")), validate it against a second anchor
 * `kernel_pmap`, and return runtime addresses (link-time + slide). If the staged
 * file is missing or does not match the running kernel, every lookup fails
 * safely (NULL) and the caller leaves the affected features disabled.
 */

#include <libkern/libkern.h>
#include <libkern/version.h>
#include <libkern/OSMalloc.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <string.h>

#include <libklookup/klookup.h>

#define KL_STAGED_PATH  "/var/db/procfs.ksyms"
#define KL_MAX          8192        /* the staged file is a few hundred bytes */

/* Validation anchor: a known kernel data symbol whose runtime address we have. */
extern pmap_t kernel_pmap;

static OSMallocTag g_kl_tag = NULL;

/*
 * Read up to `cap` bytes of `path` into `buf`; sets *outlen to the bytes read.
 */
static int
kl_slurp(const char *path, char *buf, size_t cap, size_t *outlen, vfs_context_t ctx)
{
    vnode_t vp = NULLVP;
    if (vnode_lookup(path, 0, &vp, ctx) != 0 || vp == NULLVP) {
        return ENOENT;
    }
    uio_t uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
    if (uio == NULL) {
        vnode_put(vp);
        return ENOMEM;
    }
    uio_addiov(uio, (user_addr_t)(uintptr_t)buf, cap);
    int err = VNOP_READ(vp, uio, 0, ctx);
    *outlen = cap - (size_t)uio_resid(uio);
    uio_free(uio);
    vnode_put(vp);
    return err;
}

/*
 * Find a "<name> 0x<hex>" line in the staged buffer and return the hex value, or
 * 0 if absent.
 */
static uint64_t
kl_line_value(const char *buf, size_t len, const char *name)
{
    size_t nl = strlen(name);
    const char *p = buf;
    const char *end = buf + len;

    while (p < end) {
        const char *eol = memchr(p, '\n', (size_t)(end - p));
        const char *lend = eol ? eol : end;
        if ((size_t)(lend - p) > nl + 1 &&
            strncmp(p, name, nl) == 0 && p[nl] == ' ') {
            const char *h = p + nl + 1;
            if (h + 2 <= lend && h[0] == '0' && (h[1] == 'x' || h[1] == 'X')) {
                h += 2;
            }
            uint64_t v = 0;
            while (h < lend) {
                char c = *h++;
                int d;
                if (c >= '0' && c <= '9') {
                    d = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    d = c - 'a' + 10;
                } else if (c >= 'A' && c <= 'F') {
                    d = c - 'A' + 10;
                } else {
                    break;
                }
                v = (v << 4) | (uint64_t)d;
            }
            return v;
        }
        p = eol ? eol + 1 : end;
    }
    return 0;
}

/*
 * Resolve `count` symbol names to their runtime addresses (out[i] = address or
 * NULL). Returns the number resolved.
 */
int
klookup_resolve(const char *const *names, void **out, int count)
{
    for (int i = 0; i < count; i++) {
        out[i] = NULL;
    }

    if (g_kl_tag == NULL) {
        g_kl_tag = OSMalloc_Tagalloc("klookup", OSMT_DEFAULT);
        if (g_kl_tag == NULL) {
            return 0;
        }
    }

    char *buf = OSMalloc(KL_MAX, g_kl_tag);
    if (buf == NULL) {
        return 0;
    }

    vfs_context_t ctx = vfs_context_create(NULL);
    size_t len = 0;
    int err = kl_slurp(KL_STAGED_PATH, buf, KL_MAX - 1, &len, ctx);
    vfs_context_rele(ctx);
    if (err != 0 || len == 0) {
        printf("KLOOKUP: staged symbols unavailable (%s)\n", KL_STAGED_PATH);
        OSFree(buf, KL_MAX, g_kl_tag);
        return 0;
    }
    buf[len] = '\0';

    /* Recover the slide from `version`; validate against `kernel_pmap`. */
    uint64_t version_raw = kl_line_value(buf, len, "_version");
    uint64_t kpmap_raw   = kl_line_value(buf, len, "_kernel_pmap");
    if (version_raw == 0) {
        printf("KLOOKUP: staged file has no _version anchor\n");
        OSFree(buf, KL_MAX, g_kl_tag);
        return 0;
    }
    int64_t slide = (int64_t)(uintptr_t)(void *)version - (int64_t)version_raw;
    if (kpmap_raw != 0 &&
        (uintptr_t)(kpmap_raw + slide) != (uintptr_t)(void *)&kernel_pmap) {
        printf("KLOOKUP: staged symbols do not match running kernel\n");
        OSFree(buf, KL_MAX, g_kl_tag);
        return 0;
    }

    int resolved = 0;
    for (int w = 0; w < count; w++) {
        uint64_t raw = kl_line_value(buf, len, names[w]);
        if (raw != 0) {
            out[w] = (void *)(uintptr_t)((int64_t)raw + slide);
            resolved++;
        }
    }
    printf("KLOOKUP: resolved %d/%d (slide=0x%llx)\n",
           resolved, count, (unsigned long long)slide);

    OSFree(buf, KL_MAX, g_kl_tag);
    return resolved;
}

/*
 * Single-symbol convenience wrapper around klookup_resolve().
 */
void *
SymbolLookup(const char *Symbol)
{
    void *addr = NULL;
    klookup_resolve(&Symbol, &addr, 1);
    return addr;
}
