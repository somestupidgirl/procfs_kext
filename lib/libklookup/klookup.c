/*
 * klookup.c
 *
 * Resolve private kernel symbols from the on-disk kernel collection.
 *
 * SPDX-License-Identifier: 0BSD
 *
 * Copyright (c) 2021 Syncretic
 * Copyright (c) 2024-2026 Sunneva N. Mariu
 *
 * The running kernel jettisons its __LINKEDIT (and thus its symbol table) after
 * boot to reclaim memory, so the in-memory Mach-O symtab is unavailable on
 * Apple Silicon. The booted kernel collection on disk
 * (/System/Library/KernelCollections/BootKernelExtensions.kc) still contains the
 * full symbol table. We read the com.apple.kernel slice's symtab/strtab from
 * that file, look symbols up by name, and add the running kernel's KASLR slide
 * (recovered from `version`) to each link-time n_value to get its runtime
 * address. Validate by checking that "_version" resolves to &version.
 */

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <libkern/version.h>
#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <string.h>

#include <libklookup/klookup.h>

#define KL_KC_PATH  "/System/Library/KernelCollections/BootKernelExtensions.kc"
#define KL_HDR_MAX  0x10000     /* covers the fileset + slice load commands */

/* Second calibration anchor: a known kernel data symbol whose runtime address
 * we have, used to confirm the .kc matches the running kernel. */
extern pmap_t kernel_pmap;

static OSMallocTag g_kl_tag = NULL;

/*
 * Read exactly `len` bytes from `vp` at file offset `off` into `buf`, looping
 * over short reads. Returns 0 on success.
 */
static int
kl_read(vnode_t vp, off_t off, void *buf, user_size_t len, vfs_context_t ctx)
{
    user_size_t done = 0;
    while (done < len) {
        uio_t uio = uio_create(1, off + (off_t)done, UIO_SYSSPACE, UIO_READ);
        if (uio == NULL) {
            return ENOMEM;
        }
        uio_addiov(uio, (user_addr_t)(uintptr_t)((uint8_t *)buf + done), len - done);
        int err = VNOP_READ(vp, uio, 0, ctx);
        user_ssize_t resid = uio_resid(uio);
        uio_free(uio);
        if (err != 0) {
            return err;
        }
        user_size_t got = (len - done) - (user_size_t)resid;
        if (got == 0) {
            return EIO;     /* no progress: short read / EOF */
        }
        done += got;
    }
    return 0;
}

/*
 * Find the file offset of the "com.apple.kernel" slice in a fileset header.
 */
static int
kl_find_kernel_slice(const uint8_t *hdr, size_t hlen, uint64_t *out_fileoff)
{
    const struct mach_header_64 *mh = (const struct mach_header_64 *)hdr;
    size_t off = sizeof(*mh);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (off + sizeof(struct load_command) > hlen) {
            break;
        }
        const struct load_command *lc = (const struct load_command *)(hdr + off);
        if (lc->cmdsize == 0 || off + lc->cmdsize > hlen) {
            break;
        }
        if (lc->cmd == LC_FILESET_ENTRY) {
            const struct fileset_entry_command *fe =
                (const struct fileset_entry_command *)lc;
            const char *name = (const char *)lc + fe->entry_id.offset;
            if ((const uint8_t *)name < hdr + hlen &&
                strcmp(name, "com.apple.kernel") == 0) {
                *out_fileoff = fe->fileoff;
                return 1;
            }
        }
        off += lc->cmdsize;
    }
    return 0;
}

/*
 * Find LC_SYMTAB in a Mach-O header buffer.
 */
static const struct symtab_command *
kl_find_symtab(const uint8_t *hdr, size_t hlen)
{
    const struct mach_header_64 *mh = (const struct mach_header_64 *)hdr;
    if (mh->magic != MH_MAGIC_64) {
        return NULL;
    }
    size_t off = sizeof(*mh);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (off + sizeof(struct load_command) > hlen) {
            break;
        }
        const struct load_command *lc = (const struct load_command *)(hdr + off);
        if (lc->cmdsize == 0 || off + lc->cmdsize > hlen) {
            break;
        }
        if (lc->cmd == LC_SYMTAB) {
            return (const struct symtab_command *)lc;
        }
        off += lc->cmdsize;
    }
    return NULL;
}

/*
 * Resolve `count` symbol names to their runtime addresses. out[i] is set to the
 * resolved address or NULL. Returns the number resolved. Reads the kernelcache
 * once, so resolve everything you need in a single call.
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

    vfs_context_t ctx = vfs_context_create(NULL);
    vnode_t vp = NULLVP;
    if (vnode_lookup(KL_KC_PATH, 0, &vp, ctx) != 0 || vp == NULLVP) {
        printf("KLOOKUP: cannot open %s\n", KL_KC_PATH);
        vfs_context_rele(ctx);
        return 0;
    }

    int resolved = 0;
    uint8_t          *hdr     = OSMalloc(KL_HDR_MAX, g_kl_tag);
    uint8_t          *slice   = OSMalloc(KL_HDR_MAX, g_kl_tag);
    struct nlist_64  *syms    = NULL;
    char             *strs    = NULL;
    uint32_t          symsize = 0;
    uint32_t          strsize = 0;

    if (hdr == NULL || slice == NULL) {
        goto out;
    }
    if (kl_read(vp, 0, hdr, KL_HDR_MAX, ctx) != 0) {
        printf("KLOOKUP: header read failed\n");
        goto out;
    }

    /* Locate LC_SYMTAB: in the kernel slice for a fileset, else the main image. */
    const struct mach_header_64 *mh = (const struct mach_header_64 *)hdr;
    const struct symtab_command *st = NULL;
    if (mh->filetype == MH_FILESET) {
        uint64_t slice_fo = 0;
        if (!kl_find_kernel_slice(hdr, KL_HDR_MAX, &slice_fo)) {
            printf("KLOOKUP: com.apple.kernel slice not found\n");
            goto out;
        }
        if (kl_read(vp, (off_t)slice_fo, slice, KL_HDR_MAX, ctx) != 0) {
            goto out;
        }
        st = kl_find_symtab(slice, KL_HDR_MAX);
    } else {
        st = kl_find_symtab(hdr, KL_HDR_MAX);
    }
    if (st == NULL || st->nsyms == 0 || st->strsize == 0) {
        printf("KLOOKUP: LC_SYMTAB not found\n");
        goto out;
    }

    symsize = (uint32_t)(st->nsyms * sizeof(struct nlist_64));
    strsize = st->strsize;
    uint32_t symoff = st->symoff;
    uint32_t stroff = st->stroff;
    uint32_t nsyms  = st->nsyms;

    syms = OSMalloc(symsize, g_kl_tag);
    strs = OSMalloc(strsize, g_kl_tag);
    if (syms == NULL || strs == NULL) {
        printf("KLOOKUP: alloc failed (sym=%u str=%u)\n", symsize, strsize);
        goto out;
    }
    if (kl_read(vp, symoff, syms, symsize, ctx) != 0 ||
        kl_read(vp, stroff, strs, strsize, ctx) != 0) {
        printf("KLOOKUP: symtab/strtab read failed\n");
        goto out;
    }

    /*
     * Capture raw (link-time) n_values for the requested names, and for two
     * known data anchors. The .kc may be linked at a different base than the
     * running kernel, so derive the slide from `version` (whose runtime address
     * we know) and validate it against `kernel_pmap` (a second known address):
     * if both anchors agree, the .kc matches the running build and every other
     * resolved address is correct too.
     */
    uint64_t version_raw = 0;
    uint64_t kpmap_raw   = 0;
    for (uint32_t i = 0; i < nsyms; i++) {
        uint32_t strx = syms[i].n_un.n_strx;
        if (strx == 0 || strx >= strsize || syms[i].n_value == 0) {
            continue;
        }
        const char *nm = strs + strx;
        if (version_raw == 0 && strcmp(nm, "_version") == 0) {
            version_raw = syms[i].n_value;
        }
        if (kpmap_raw == 0 && strcmp(nm, "_kernel_pmap") == 0) {
            kpmap_raw = syms[i].n_value;
        }
        for (int w = 0; w < count; w++) {
            if (out[w] == NULL && strcmp(nm, names[w]) == 0) {
                out[w] = (void *)(uintptr_t)syms[i].n_value;  /* raw for now */
            }
        }
    }

    if (version_raw == 0) {
        printf("KLOOKUP: no _version anchor in symtab\n");
        for (int w = 0; w < count; w++) out[w] = NULL;
        goto out;
    }

    int64_t slide = (int64_t)(uintptr_t)(void *)version - (int64_t)version_raw;

    if (kpmap_raw != 0 &&
        (uintptr_t)(kpmap_raw + slide) != (uintptr_t)(void *)&kernel_pmap) {
        printf("KLOOKUP: anchor mismatch - .kc does not match running kernel\n");
        for (int w = 0; w < count; w++) out[w] = NULL;
        goto out;
    }

    for (int w = 0; w < count; w++) {
        if (out[w] != NULL) {
            out[w] = (void *)(uintptr_t)((int64_t)(uintptr_t)out[w] + slide);
            resolved++;
        }
    }
    printf("KLOOKUP: resolved %d/%d (slide=0x%llx, kpmap_ok=%d)\n",
           resolved, count, (unsigned long long)slide, kpmap_raw != 0);

out:
    if (strs != NULL) {
        OSFree(strs, strsize, g_kl_tag);
    }
    if (syms != NULL) {
        OSFree(syms, symsize, g_kl_tag);
    }
    if (slice != NULL) {
        OSFree(slice, KL_HDR_MAX, g_kl_tag);
    }
    if (hdr != NULL) {
        OSFree(hdr, KL_HDR_MAX, g_kl_tag);
    }
    vnode_put(vp);
    vfs_context_rele(ctx);
    return resolved;
}

/*
 * Convenience single-symbol lookup. Prefer klookup_resolve() for batches: this
 * re-reads the kernelcache on every call.
 */
void *
SymbolLookup(const char *Symbol)
{
    void *addr = NULL;
    klookup_resolve(&Symbol, &addr, 1);
    return addr;
}
