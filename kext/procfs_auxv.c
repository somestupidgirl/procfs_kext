/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_auxv.c
 *
 * Process auxiliary-vector node (/proc/<pid>/auxv).
 *
 * macOS executables are Mach-O, not ELF, so there is no classic ELF auxiliary
 * vector (the AT_* key/value array) on the user stack. XNU's direct equivalent
 * is dyld's "apple[]" array: the NUL-terminated key=value strings the kernel
 * places on the stack immediately after argv and envp (executable_path=,
 * stack_guard=, malloc_entropy=, ptr_munge=, dyld_file=, main_stack=,
 * arm64e_abi=, ...). This node emits that array - XNU's auxv equivalent.
 *
 * The stack layout at process entry (LP64) is:
 *
 *   [ argc ][ argv[0..n] NULL ][ envp[0..m] NULL ][ apple[0..k] NULL ][ strings ]
 *
 * so the apple pointers are found by skipping the argc slot, then argv up to its
 * NULL, then envp up to its NULL. Each apple pointer is followed into the string
 * region. All reads go through the target's pmap via procfs_copy_user_phys()
 * (shared with procfs_cmdline.c) - the same linkable path KERN_PROCARGS2 avoids
 * needing the private vm_map_copyin() KPI.
 *
 * A human-readable, Linux-style AT_* rendering is provided separately, guarded,
 * in procfs_linux.c for the planned native-vs-linux compatibility mode.
 */
#include <stdint.h>
#include <string.h>

#include <mach/vm_types.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/uio.h>

#include <bsdcompat/sys/malloc.h>

#include <fs/procfs/procfs.h>

/* Shared with procfs_cmdline.c: best-effort read of user memory via the target's
 * pmap, returning the number of leading resident bytes copied. */
extern size_t procfs_copy_user_phys(pmap_t pmap, user_addr_t uva, uint8_t *dst, size_t len);
extern pmap_t get_task_pmap(task_t task);

#define AUXV_PTRSZ        ((user_addr_t)sizeof(uint64_t))   /* LP64 pointer */
#define AUXV_MAX_PTRS     8192      /* cap on argv/envp/apple entries scanned */
#define AUXV_MAX_STRLEN   1024      /* cap on one apple string */
#define AUXV_MAX_TOTAL    (128 * 1024)

/* Read one LP64 pointer from user va. Returns FALSE if the page is not resident. */
static boolean_t
auxv_read_ptr(pmap_t pmap, user_addr_t va, user_addr_t *out)
{
    uint64_t v = 0;
    if (procfs_copy_user_phys(pmap, va, (uint8_t *)&v, sizeof(v)) != sizeof(v)) {
        return FALSE;
    }
    *out = (user_addr_t)v;
    return TRUE;
}

/* Advance *off past a NULL-terminated pointer array. Returns FALSE on a read
 * failure or if the array is implausibly long. */
static boolean_t
auxv_skip_ptr_array(pmap_t pmap, user_addr_t *off)
{
    for (int i = 0; i < AUXV_MAX_PTRS; i++) {
        user_addr_t ptr;
        if (!auxv_read_ptr(pmap, *off, &ptr)) {
            return FALSE;
        }
        *off += AUXV_PTRSZ;
        if (ptr == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * Collect the target's apple[] strings into a freshly malloc'd buffer, each
 * string NUL-terminated. Returns the buffer (caller frees with M_TEMP) and sets
 * *outlen, or NULL on failure / nothing readable.
 */
static uint8_t *
procfs_read_apple_array(proc_t p, size_t *outlen)
{
    *outlen = 0;

    user_addr_t sp   = p->user_stack;
    task_t      task = proc_task(p);
    pmap_t      pmap = (task != TASK_NULL) ? get_task_pmap(task) : NULL;
    if (sp == 0 || pmap == NULL) {
        return NULL;
    }

    /* Skip the argc slot, then the argv and envp pointer arrays; what remains is
     * the apple[] pointer array. */
    user_addr_t off = sp + AUXV_PTRSZ;
    if (!auxv_skip_ptr_array(pmap, &off) ||     /* argv */
        !auxv_skip_ptr_array(pmap, &off)) {     /* envp */
        return NULL;
    }

    uint8_t *buf = malloc(AUXV_MAX_TOTAL, M_TEMP, M_WAITOK);
    if (buf == NULL) {
        return NULL;
    }

    size_t used = 0;
    for (int i = 0; i < AUXV_MAX_PTRS; i++) {
        user_addr_t ptr;
        if (!auxv_read_ptr(pmap, off, &ptr)) {
            break;
        }
        off += AUXV_PTRSZ;
        if (ptr == 0) {
            break;                              /* end of apple[] */
        }

        char tmp[AUXV_MAX_STRLEN];
        size_t got = procfs_copy_user_phys(pmap, ptr, (uint8_t *)tmp, sizeof(tmp));
        if (got == 0) {
            continue;                           /* not resident - skip */
        }
        size_t slen = strnlen(tmp, got);
        if (used + slen + 1 > AUXV_MAX_TOTAL) {
            break;
        }
        memcpy(buf + used, tmp, slen);
        used += slen;
        buf[used++] = '\0';
    }

    if (used == 0) {
        free(buf, M_TEMP);
        return NULL;
    }
    *outlen = used;
    return buf;
}

int
procfs_doauxv(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    if (uio_rw(uio) != UIO_READ) {
        return EOPNOTSUPP;       /* the node is read-only */
    }

    /* Linux presentation mode renders synthesized AT_* text (procfs_linux.c). */
    if (procfs_linux_mode) {
        return procfs_doauxv_linux(pnp, uio, ctx);
    }

    proc_t p = proc_find(pnp->node_id.nodeid_pid);
    if (p == PROC_NULL) {
        return ESRCH;
    }

    /* Zombies and system processes have no user stack to read from. */
    if ((p->p_stat == SZOMB) || (p->p_flag & P_SYSTEM) != 0) {
        proc_rele(p);
        return procfs_copy_data("", 0, uio);    /* empty (EOF) */
    }

    size_t   len = 0;
    uint8_t *buf = procfs_read_apple_array(p, &len);

    int error;
    if (buf == NULL) {
        error = procfs_copy_data("", 0, uio);   /* nothing readable - empty */
    } else {
        error = procfs_copy_data((const char *)buf, (int)len, uio);
        free(buf, M_TEMP);
    }

    proc_rele(p);
    return error;
}
