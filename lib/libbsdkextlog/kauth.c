/*
 * Created 190415 lynnl
 *
 * see:
 *  https://developer.apple.com/library/archive/technotes/tn2127/_index.html
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/vnode.h>

#include "kauth.h"
#include "utils.h"
#include "log_kctl.h"

static inline const char *vtype_string(enum vtype vt)
{
    static const char *vtypes[] = {
            "VNON", "VREG", "VDIR", "VBLK", "VCHR", "VLNK",
            "VSOCK", "VFIFO", "VBAD", "VSTR", "VCPLX",
    };
    return vt < ARRAY_SIZE(vtypes) ? vtypes[vt] : "(?)";
}

typedef struct {
    errno_t e;
    int len;        /* strlen(path) */
    char *path;
} vnode_path_t;

/**
 * Get path of vnode(one vnode may have more than one path)
 * @vp          a vnode pointer
 * @return      a struct contains info about the vnode path
 *              XXX: must check vnode_path_t.e before use
 *              you're responsible to free vnode_path_t.path after use
 */
static vnode_path_t make_vnode_path(vnode_t vp)
{
    int e;
    int len = PATH_MAX;     /* Don't touch */
    char *path = NULL;

    kassert_nonnull(vp);

    /*
     * NOTE:
     *  For compatibility reason
     *  Length of the path should(and must) be PATH_MAX(1024 bytes)
     *
     * References:
     *  developer.apple.com/legacy/library/technotes/tn/tn1150.html#Symlinks
     */
    path = (char *) util_malloc0(PATH_MAX, M_WAITOK | M_NULL);
    if (path == NULL) {
        e = ENOMEM;
        goto out_exit;
    }

    /*
     * There must be a NULL-terminator inside *path  don't worry
     *  len = strlen(path) + 1(EOS) in result
     *
     * NOTE:
     *  The third parameter of vn_getpath() must be initialized
     *  O.w. kernel will panic  see: xnu/bsd/vfs/vfs_subr.c
     */
    e = vn_getpath(vp, path, &len);
    if (e == 0) {
        kassertf(len > 0, "non-positive len %d", len);
        len--;      /* Don't count trailing '\0' */
    } else {
        util_mfree(path);
        path = NULL;
    }

out_exit:
    return (vnode_path_t) {e, len, path};
}

static inline const char *generic_action_str(kauth_action_t act)
{
    switch (act) {
    case KAUTH_GENERIC_ISSUSER:
        return "ISSUSER";
    }
    return "(?)";
}

static int generic_scope_cb(
        kauth_cred_t cred,
        void *idata,
        kauth_action_t act,
        uintptr_t arg0,
        uintptr_t arg1,
        uintptr_t arg2,
        uintptr_t arg3)
{
    uid_t uid;
    int pid;
    char pcomm[MAXCOMLEN + 1];

    if (kcb_get() < 0) goto out_put;

    UNUSED(idata, arg0, arg1, arg2, arg3);

    uid = kauth_cred_getuid(cred);
    pid = proc_selfpid();
    proc_selfname(pcomm, sizeof(pcomm));

    log_info("generic  act: %#x(%s) uid: %u pid: %d %s",
             act, generic_action_str(act), uid, pid, pcomm);

out_put:
    (void) kcb_put();
    return KAUTH_RESULT_DEFER;
}

static inline const char *process_action_str(kauth_action_t act)
{
    switch (act) {
    case KAUTH_PROCESS_CANSIGNAL:
        return "CANSIGNAL";
    case KAUTH_PROCESS_CANTRACE:
        return "CANTRACE";
    }
    return "(?)";
}

static int process_scope_cb(
        kauth_cred_t cred,
        void *idata,
        kauth_action_t act,
        uintptr_t arg0,
        uintptr_t arg1,
        uintptr_t arg2,
        uintptr_t arg3)
{
    uid_t uid;
    int pid;
    char pcomm[MAXCOMLEN + 1];

    proc_t proc;
    int pid2;
    char pcomm2[MAXCOMLEN + 1];
    int signal;

    if (kcb_get() < 0) goto out_put;

    UNUSED(idata, arg2, arg3);

    uid = kauth_cred_getuid(cred);
    pid = proc_selfpid();
    proc_selfname(pcomm, sizeof(pcomm));

    switch (act) {
    case KAUTH_PROCESS_CANSIGNAL:
        proc = (proc_t) arg0;
        signal = (int) arg1;
        pid2 = proc_pid(proc);
        proc_name(pid2, pcomm2, sizeof(pcomm2));

        log_info("process  act: %#x(%s) uid: %u pid: %d %s dst: %d %s sig: %d",
                act, process_action_str(act), uid, pid, pcomm, pid2, pcomm2, signal);
        break;

    case KAUTH_PROCESS_CANTRACE:
        proc = (proc_t) arg0;
        pid2 = proc_pid(proc);
        proc_name(pid2, pcomm2, sizeof(pcomm2));

        log_warning("process  act: %#x(%s) uid: %u pid: %d %s dst: %d %s",
                act, process_action_str(act), uid, pid, pcomm, pid2, pcomm2);
        break;

    default:
        log_warning("unknown action %#x in process scope", act);
        break;
    }

out_put:
    (void) kcb_put();
    return KAUTH_RESULT_DEFER;
}

#define GET_TYPE_STR     0
#define GET_TYPE_LEN     1

static inline void *vn_act_str_one(
        int type,
        kauth_action_t a,
        bool isdir)
{
    char *p;

    kassertf(type >= GET_TYPE_STR && type <= GET_TYPE_LEN, "Bad type %#x", type);
    kassert_ne(a, 0, "%d", "%d");
    kassertf((a & (a - 1)) == 0, "Only one bit should be set, got %#x", a);

    switch (a) {
    case KAUTH_VNODE_READ_DATA:
        BUILD_BUG_ON(KAUTH_VNODE_READ_DATA != KAUTH_VNODE_LIST_DIRECTORY);
        p = isdir ? "LIST_DIRECTORY" : "READ_DATA";
        break;
    case KAUTH_VNODE_WRITE_DATA:
        BUILD_BUG_ON(KAUTH_VNODE_WRITE_DATA != KAUTH_VNODE_ADD_FILE);
        p = isdir ? "ADD_FILE" : "WRITE_DATA";
        break;
    case KAUTH_VNODE_EXECUTE:
        BUILD_BUG_ON(KAUTH_VNODE_EXECUTE != KAUTH_VNODE_SEARCH);
        p = isdir ? "SEARCH" : "EXECUTE";
        break;
    case KAUTH_VNODE_DELETE:
        p = "DELETE";
        break;
    case KAUTH_VNODE_APPEND_DATA:
        BUILD_BUG_ON(KAUTH_VNODE_APPEND_DATA != KAUTH_VNODE_ADD_SUBDIRECTORY);
        p = isdir ? "ADD_SUBDIRECTORY" : "APPEND_DATA";
        break;
    case KAUTH_VNODE_DELETE_CHILD:
        p = "DELETE_CHILD";
        break;
    case KAUTH_VNODE_READ_ATTRIBUTES:
        p = "READ_ATTRIBUTES";
        break;
    case KAUTH_VNODE_WRITE_ATTRIBUTES:
        p = "WRITE_ATTRIBUTES";
        break;
    case KAUTH_VNODE_READ_EXTATTRIBUTES:
        p = "READ_EXTATTRIBUTES";
        break;
    case KAUTH_VNODE_WRITE_EXTATTRIBUTES:
        p = "WRITE_EXTATTRIBUTES";
        break;
    case KAUTH_VNODE_READ_SECURITY:
        p = "READ_SECURITY";
        break;
    case KAUTH_VNODE_WRITE_SECURITY:
        p = "WRITE_SECURITY";
        break;
    case KAUTH_VNODE_TAKE_OWNERSHIP:
        BUILD_BUG_ON(KAUTH_VNODE_TAKE_OWNERSHIP != KAUTH_VNODE_CHANGE_OWNER);
        p = "TAKE_OWNERSHIP";
        break;
    case KAUTH_VNODE_SYNCHRONIZE:
        p = "SYNCHRONIZE";
        break;
    case KAUTH_VNODE_LINKTARGET:
        p = "LINKTARGET";
        break;
    case KAUTH_VNODE_CHECKIMMUTABLE:
        p = "CHECKIMMUTABLE";
        break;
    case KAUTH_VNODE_ACCESS:
        p = "ACCESS";
        break;
    case KAUTH_VNODE_NOIMMUTABLE:
        p = "NOIMMUTABLE";
        break;
    case KAUTH_VNODE_SEARCHBYANYONE:
        p = "SEARCHBYANYONE";
        break;
    default:
        /* Fallback: use question mark for unrecognized bit */
        p = "?";
        break;
    }

    return type == GET_TYPE_STR ? p : (void *) strlen(p);
}

static inline int ffs_zero(int x)
{
    kassert_ne(x, 0, "%d", "%d");
    return __builtin_ffs(x) - 1;
}

/**
 * Format vnode action into string
 * @action      The vnode action
 * @return      String to describe all bits in action
 *              NULL if OOM
 *              You're responsible to free the space via util_mfree()
 */
static inline char * __nullable vn_act_str(kauth_action_t act, vnode_t vp)
{
    kauth_action_t a;
    bool isdir;
    int i, n, size;
    char *str;

    kassert_nonnull(vp);

    isdir = vnode_isdir(vp);

    a = act;
    size = 1;
    while (a != 0) {
        size += (int) vn_act_str_one(GET_TYPE_LEN, 1 << ffs_zero(a), isdir);
        a &= a - 1;
        if (a != 0) size++;    /* Add a pipe separator */
    }

    str = util_malloc0(size, M_WAITOK | M_NULL);
    if (str == NULL) goto out_exit;

    a = act;
    i = 0;
    while (a != 0) {
        n = snprintf(str + i, size - i, "%s", vn_act_str_one(GET_TYPE_STR, 1 << ffs_zero(a), isdir));
        kassert_gt(n, 0, "%d", "%d");
        i += n;
        a &= a - 1;
        if (a != 0) str[i++] = '|';
    }

    /* In case act is zero, we have to terminate the string manually */
    if (size == 1) *str = '\0';
    kassert_eq(i + 1, size, "%d", "%d");
    kassert_eq(str[i], '\0', "%#x", "%#x");
    out_exit:
    return str;
}

static int vnode_scope_cb(
        kauth_cred_t cred,
        void *idata,
        kauth_action_t act,
        uintptr_t arg0,
        uintptr_t arg1,
        uintptr_t arg2,
        uintptr_t arg3)
{
    vfs_context_t ctx;
    vnode_t vp;
    vnode_t dvp;

    uid_t uid;
    int pid;
    char pcomm[MAXCOMLEN + 1];
    char *str;
    enum vtype vt;

    vnode_path_t vpath;

    if (kcb_get() < 0) goto out_put;

    UNUSED(idata, arg3);   /* XXX: TODO? */

    ctx = (vfs_context_t) arg0;
    UNUSED(ctx);
    vp = (vnode_t) arg1;
    dvp = (vnode_t) arg2;           /* may NULLVP(alias of NULL) */

    uid = kauth_cred_getuid(cred);
    pid = proc_selfpid();
    proc_selfname(pcomm, sizeof(pcomm));

    vpath = make_vnode_path(vp);
    vt = vnode_vtype(vp);
    if (vpath.e != 0) {
        log_error("make_vnode_path() fail  vp: %p vid: %#x vt: %d",
                    vp, vnode_vid(vp), vnode_vtype(vp));
        goto out_put;
    }

    str = vn_act_str(act, vp);
    log_info("vnode  act: %#x(%s) vp: %p %d %s %s dvp: %p uid: %u pid: %d %s",
          act, str, vp, vt, vtype_string(vt), vpath.path, dvp, uid, pid, pcomm);
    util_mfree(str);
    util_mfree(vpath.path);

out_put:
    (void) kcb_put();
    return KAUTH_RESULT_DEFER;
}

static inline const char *fileop_action_str(kauth_action_t act)
{
    switch (act) {
    case KAUTH_FILEOP_OPEN:
        return "OPEN";
    case KAUTH_FILEOP_CLOSE:
        return "CLOSE";
    case KAUTH_FILEOP_RENAME:
        return "RENAME";
    case KAUTH_FILEOP_EXCHANGE:
        return "EXCHANGE";
    case KAUTH_FILEOP_LINK:
        return "LINK";
    case KAUTH_FILEOP_EXEC:
        return "EXEC";
    case KAUTH_FILEOP_DELETE:
        return "DELETE";
#if OS_VER_MIN_REQ >= __MAC_10_14
    /* First introduced in macOS 10.14 */
    case KAUTH_FILEOP_WILL_RENAME:
        return "WILL_RENAME";
#endif
    }
    return "?";
}

/*
 * [sic Technical Note TN2127 Kernel Authorization#File Operation Scope]
 *
 * Warning:
 * Prior to Mac OS X 10.5 the file operation scope had a nasty gotcha(r. 4605516).
 * If you install a listener in the this scope and handle the
 *      KAUTH_FILEOP_RENAME,
 *      KAUTH_FILEOP_LINK, or
 *      KAUTH_FILEOP_EXEC actions,
 *  you must test whether arg0 and arg1 are NULL before accessing them as strings.
 *
 * Under certain circumstances(most notably, very early in the boot sequence
 *  and very late in the shutdown sequence),
 *  the kernel might pass you NULL for these arguments.
 *
 * If you access such a pointer as a string, you will kernel panic.
 */
static int fileop_scope_cb(
        kauth_cred_t cred,
        void *idata,
        kauth_action_t act,
        uintptr_t arg0,
        uintptr_t arg1,
        uintptr_t arg2,
        uintptr_t arg3)
{
    uid_t uid;
    int pid;
    char pcomm[MAXCOMLEN + 1];

    vnode_t vp;
    const char *path1;
    const char *path2;
    int flags;

    if (kcb_get() < 0) goto out_put;

    UNUSED(idata, arg3);

    uid = kauth_cred_getuid(cred);
    pid = proc_selfpid();
    proc_selfname(pcomm, sizeof(pcomm));

    switch (act) {
    case KAUTH_FILEOP_OPEN:
        vp = (vnode_t) arg0;
        path1 = (char *) arg1;

        log_info("fileop  act: %#x(%s) vp: %p %d %s uid: %u pid: %d %s",
                act, fileop_action_str(act), vp, vnode_vtype(vp), path1, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_CLOSE:
        vp = (vnode_t) arg0;
        path1 = (char *) arg1;
        flags = (int) arg2;

        log_info("fileop  act: %#x(%s) vp: %p %d %s flags: %#x uid: %u pid: %d %s",
                act, fileop_action_str(act), vp, vnode_vtype(vp), path1, flags, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_RENAME:
        path1 = (char * _Nullable) arg0;
        path2 = (char * _Nullable) arg1;

        log_info("fileop  act: %#x(%s) %s -> %s uid: %u pid: %d %s",
                act, fileop_action_str(act), path1, path2, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_EXCHANGE:
        path1 = (char *) arg0;
        path2 = (char *) arg1;

        log_info("fileop  act: %#x(%s) %s <=> %s uid: %u pid: %d %s",
                act, fileop_action_str(act), path1, path2, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_LINK:
        path1 = (char * _Nullable) arg0;
        path2 = (char * _Nullable) arg1;

        log_info("fileop  act: %#x(%s) %s ~> %s uid: %u pid: %d %s",
                act, fileop_action_str(act), path1, path2, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_EXEC:
        vp = (vnode_t) arg0;
        path1 = (char * _Nullable) arg1;

        log_info("fileop  act: %#x(%s) vp: %p %d %s uid: %u pid: %d %s",
                act, fileop_action_str(act), vp, vnode_vtype(vp), path1, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_DELETE:
        vp = (vnode_t) arg0;
        path1 = (char *) arg1;

        log_info("fileop  act: %#x(%s) vp: %p %d %s uid: %u pid: %d %s",
                act, fileop_action_str(act), vp, vnode_vtype(vp), path1, uid, pid, pcomm);
        break;

#if OS_VER_MIN_REQ >= __MAC_10_14
    /* First introduced in macOS 10.14 */
    case KAUTH_FILEOP_WILL_RENAME:
        vp = (vnode_t) arg0;
        path1 = (char *) arg1;
        path2 = (char *) arg2;

        log_info("fileop  act: %#x(%s) vp: %p %d %s -> %s uid: %u pid: %d %s",
                act, fileop_action_str(act), vp, vnode_vtype(vp), path1, path2, uid, pid, pcomm);
        break;
#endif

    default:
        log_warning("unknown action %#x in fileop scope", act);
        break;
    }

out_put:
    (void) kcb_put();
    return KAUTH_RESULT_DEFER;
}

static const char *scope_name[] = {
    KAUTH_SCOPE_GENERIC,
    KAUTH_SCOPE_PROCESS,
    KAUTH_SCOPE_VNODE,
    KAUTH_SCOPE_FILEOP,
};

static kauth_scope_callback_t scope_cb[] = {
    generic_scope_cb,
    process_scope_cb,
    vnode_scope_cb,
    fileop_scope_cb,
};

static kauth_listener_t scope_ref[] = {
    NULL, NULL, NULL, NULL,
};

kern_return_t kauth_register(void)
{
    kern_return_t r = KERN_SUCCESS;
    int i;

    BUILD_BUG_ON(ARRAY_SIZE(scope_name) != ARRAY_SIZE(scope_cb));
    BUILD_BUG_ON(ARRAY_SIZE(scope_name) != ARRAY_SIZE(scope_ref));

    for (i = 0; i < (int) ARRAY_SIZE(scope_name); i++) {
        SUPPRESS_WARN_DEPRECATED_DECL_BEGIN
        scope_ref[i] = kauth_listen_scope(scope_name[i], scope_cb[i], NULL);
        SUPPRESS_WARN_DEPRECATED_DECL_END

        if (scope_ref[i] == NULL) {
            r = KERN_FAILURE;
            log_error("kauth_listen_scope() fail  scope: %s", scope_name[i]);
            kauth_deregister();
            break;
        }
    }

    return r;
}

void kauth_deregister(void)
{
    int i;

    for (i = 0; i < (int) ARRAY_SIZE(scope_name); i++) {
        if (scope_ref[i] != NULL) {
            SUPPRESS_WARN_DEPRECATED_DECL_BEGIN
            kauth_unlisten_scope(scope_ref[i]);
            SUPPRESS_WARN_DEPRECATED_DECL_END

            scope_ref[i] = NULL;
        }
    }

    kcb_invalidate();
}

