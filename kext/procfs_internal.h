#ifndef procfs_internal_h
#define procfs_internal_h

#include <sys/filedesc.h>

struct proc;
struct pgrp;
struct session;

#pragma mark -
#pragma mark Process NULL Pointers

#define PROC_NULL               (struct proc *)NULL
#define PGRP_NULL               (struct pgrp *)NULL
#define SESSION_NULL            (struct session *)NULL

#pragma mark -
#pragma mark PID Definitions

/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 */
extern int (*_nprocs);
extern int (*_maxproc);
extern int (*_maxprocperuid);
extern int (*_hard_maxproc);

#define PID_MAX                 99999
#define NO_PID                  100000

#pragma mark -
#pragma mark Process Locks

extern void (*_proc_lock)(struct proc *);
extern void (*_proc_unlock)(struct proc *);
extern void (*_proc_spinlock)(struct proc *);
extern void (*_proc_spinunlock)(struct proc *);
extern void (*_proc_list_lock)(void);
extern void (*_proc_list_unlock)(void);
extern void (*_proc_fdlock)(struct proc *);
extern void (*_proc_fdlock_spin)(struct proc *);
extern void (*_proc_fdunlock)(struct proc *);
extern void (*_proc_fdlock_assert)(proc_t p, int assertflags);

#pragma mark -
#pragma mark Process Iteration

typedef int (*proc_iterate_fn_t)(proc_t, void *);

/*
 * These are the only valid return values of `callout` functions provided to
 * process iterators.
 *
 * CLAIMED returns expect the caller to call proc_rele on the proc.  DONE
 * returns stop iterating processes early.
 */
#define PROC_RETURNED           (0)
#define PROC_RETURNED_DONE      (1)
#define PROC_CLAIMED            (2)
#define PROC_CLAIMED_DONE       (3)

#pragma mark -
#pragma mark Process Group Iteration

/*
 * pgrp_iterate walks the provided process group, calling `filterfn` with
 * `filterarg` for each process.  For processes where `filterfn` returned
 * non-zero, `callout` is called with `arg`.  If `PGRP_DROPREF` is supplied in
 * `flags`, a reference will be dropped from the process group after obtaining
 * the list of processes to call `callout` on.
 *
 * `PGMEMBERS_FOREACH` might also be used under the pgrp_lock to achieve a
 * similar effect.
 */
#define PGRP_DROPREF (1)

extern void (*_pgrp_iterate)(struct pgrp *pgrp, unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);

/*
 * proc_iterate walks the `allproc` and/or `zombproc` lists, calling `filterfn`
 * with `filterarg` for each process.  For processes where `filterfn` returned
 * non-zero, `callout` is called with `arg`.  If the `PROC_NOWAITTRANS` flag is
 * set, this function waits for transitions.
 *
 * `ALLPROC_FOREACH` or `ZOMBPROC_FOREACH` might also be used under the
 * `proc_list_lock` to achieve a similar effect.
 */
#define PROC_ALLPROCLIST        (1U << 0) /* walk the allproc list (processes not yet exited) */
#define PROC_ZOMBPROCLIST       (1U << 1) /* walk the zombie list */
#define PROC_NOWAITTRANS        (1U << 2) /* do not wait for transitions (checkdirs only) */

extern void (*_proc_iterate)(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);

#pragma mark -
#pragma mark File Descriptor Table Iteration (sys/filedesc.h)

extern struct fdt_iterator (*_fdt_next)(proc_t p, int fd, bool only_settled);

#define _fdt_foreach(fp, p) \
    for (struct fdt_iterator __fdt_it = _fdt_next(p, -1, true); \
        ((fp) = __fdt_it.fdti_fp); \
        __fdt_it = _fdt_next(p, __fdt_it.fdti_fd, true))

#pragma mark -
#pragma mark Private KPI Symbols (sys/proc_internal.h)

extern struct pgrp * (*_proc_pgrp)(proc_t);
extern struct pgrp * (*_tty_pgrp)(struct tty * tp);
extern struct session * (*_proc_session)(proc_t p);

#pragma mark -
#pragma mark Private KPI Symbols (sys/proc.h)

extern int (*_proc_starttime)(proc_t p, struct timeval *tv);
extern task_t (*_proc_task)(proc_t proc);
extern uint32_t (*_proc_getuid)(proc_t);
extern uint32_t (*_proc_getgid)(proc_t);

#pragma mark -
#pragma mark Missing Public KPI Symbols (sys/proc.h)

/* returns the 32-byte name if it exists, otherwise returns the 16-byte name */
extern char * (*_proc_best_name)(proc_t p);

/*!
 *  @function proc_gettty
 *  @abstract Copies the associated tty vnode for a given process if it exists. The caller needs to decrement the iocount of the vnode.
 *  @return 0 on success. ENOENT if the process has no associated TTY. EINVAL if arguments are NULL or vnode_getwithvid fails.
 */
extern int (*_proc_gettty)(proc_t p, vnode_t *vp);

/* this routine populates the associated tty device for a given process if it exists, returns 0 on success or else returns EINVAL */
extern int (*_proc_gettty_dev)(proc_t p, dev_t *dev);

#pragma mark -
#pragma mark Private KPI Symbols (kern/ipc_tt.h)

/* Convert from a port to a thread */
extern thread_t (*_convert_port_to_thread)(ipc_port_t port);

#pragma mark -
#pragma mark Missing Public KPI Symbols (mach/task.h)

/* Routine task_threads */
extern kern_return_t (*_task_threads)(task_t task, thread_act_array_t *threads_out, mach_msg_type_number_t *count);

#pragma mark -
#pragma mark Missing Public KPI Symbols (mach/thread_act.h)

/* Routine thread_info */
extern kern_return_t (*_thread_info)(thread_t thread, thread_flavor_t flavor, thread_info_t thread_info, mach_msg_type_number_t *thread_info_count);

#pragma mark -
#pragma mark Private KPI Symbols (sys/vnode_if.h)

/*!
 *  @function VNOP_LOOKUP
 *  @abstract Call down to a filesystem to look for a directory entry by name.
 *  @discussion VNOP_LOOKUP is the key pathway through which VFS asks a filesystem to find a file.  The vnode
 *  should be returned with an iocount to be dropped by the caller.  A VNOP_LOOKUP() calldown can come without
 *  a preceding VNOP_OPEN().
 *  @param dvp Directory in which to look up file.
 *  @param vpp Destination for found vnode.
 *  @param cnp Structure describing filename to find, reason for lookup, and various other data.
 *  @param ctx Context against which to authenticate lookup request.
 *  @return 0 for success or a filesystem-specific error.
 */
extern errno_t (*_VNOP_LOOKUP)(vnode_t, vnode_t *, struct componentname *, vfs_context_t);

/*!
 *  @function VNOP_CREATE
 *  @abstract Call down to a filesystem to create a regular file (VREG).
 *  @discussion If file creation succeeds, "vpp" should be returned with an iocount to be dropped by the caller.
 *  A VNOP_CREATE() calldown can come without a preceding VNOP_OPEN().
 *  @param dvp Directory in which to create file.
 *  @param vpp Destination for vnode for newly created file.
 *  @param cnp Description of filename to create.
 *  @param vap File creation properties, as seen in vnode_getattr().  Manipulated with VATTR_ISACTIVE, VATTR_RETURN,
 *  VATTR_SET_SUPPORTED, and so forth.
 *  @param ctx Context against which to authenticate file creation.
 *  @return 0 for success or a filesystem-specific error.
 */
extern errno_t (*_VNOP_CREATE)(vnode_t, vnode_t *, struct componentname *, struct vnode_attr *, vfs_context_t);

/*!
 *  @function VNOP_WHITEOUT
 *  @abstract Obsolete - no longer supported.
 *  @discussion Whiteouts are used to support the union filesystem, whereby one filesystem is mounted "transparently"
 *  on top of another.  A whiteout in the upper layer of a union mount is a "deletion" of a file in the lower layer;
 *  lookups will catch the whiteout and fail, setting ISWHITEOUT in the componentname structure, even if an underlying
 *  file of the same name exists.  The whiteout vnop is used for creation, deletion, and checking whether a directory
 *  supports whiteouts (see flags).
 *  also support the LOOKUP flag, which is used to test whether a directory supports whiteouts.
 *  @param dvp Directory in which to create.
 *  @param cnp Name information for whiteout.
 *  @param flags CREATE: create a whiteout. LOOKUP: check whether a directory supports whiteouts, DELETE: remove a whiteout.
 *  @param ctx Context against which to authenticate whiteout creation.
 *  @return 0 for success or a filesystem-specific error.  Returning 0 for LOOKUP indicates that a directory does support whiteouts.
 */
extern errno_t (*_VNOP_WHITEOUT)(vnode_t, struct componentname *, int, vfs_context_t);

/*!
 *  @function VNOP_MKNOD
 *  @abstract Call down to a filesystem to create a special file.
 *  @discussion The mknod vnop is used to create character and block device files, named pipe (FIFO) files, and named sockets.
 *  The newly created file should be returned with an iocount which will be dropped by the caller.  A VNOP_MKNOD() call
 *  can come down without a preceding VNOP_OPEN().
 *  @param dvp Directory in which to create the special file.
 *  @param vpp Destination for newly created vnode.
 *  @param cnp Name information for new file.
 *  @param vap Attributes for new file, including type.
 *  @param ctx Context against which to authenticate node creation.
 *  @return 0 for success or a filesystem-specific error.
 */
extern errno_t (*_VNOP_MKNOD)(vnode_t, vnode_t *, struct componentname *, struct vnode_attr *, vfs_context_t);

/*!
 *  @function VNOP_OPEN
 *  @abstract Call down to a filesystem to open a file.
 *  @discussion The open vnop gives a filesystem a chance to initialize a file for
 *  operations like reading, writing, and ioctls.  VFS promises to send down exactly one VNOP_CLOSE()
 *  for each VNOP_OPEN().
 *  @param vp File to open.
 *  @param mode FREAD and/or FWRITE.
 *  @param ctx Context against which to authenticate open.
 *  @return 0 for success or a filesystem-specific error.
 */
extern errno_t (*_VNOP_OPEN)(vnode_t vp, int mode, vfs_context_t ctx);

/*!
 *  @function VNOP_CLOSE
 *  @abstract Call down to a filesystem to close a file.
 *  @discussion The close vnop gives a filesystem a chance to release state set up
 *  by a VNOP_OPEN(). VFS promises to send down exactly one VNOP_CLOSE() for each VNOP_OPEN().
 *  @param vp File to close.
 *  @param fflag FREAD and/or FWRITE; in the case of a file opened with open(2), fflag corresponds
 *  to how the file was opened.
 *  @param ctx Context against which to authenticate close.
 *  @return 0 for success or a filesystem-specific error.
 */
extern errno_t (*_VNOP_CLOSE)(vnode_t, int, vfs_context_t);

/*!
 *  @function VNOP_ACCESS
 *  @abstract Call down to a filesystem to see if a kauth-style operation is permitted.
 *  @discussion VNOP_ACCESS is currently only called on filesystems which mark themselves
 *  as doing their authentication remotely (vfs_setauthopaque(), vfs_authopaque()).  A VNOP_ACCESS()
 *  calldown may come without any preceding VNOP_OPEN().
 *  @param vp File to authorize action for.
 *  @param action kauth-style action to be checked for permissions, e.g. KAUTH_VNODE_DELETE.
 *  @param ctx Context against which to authenticate action.
 *  @return 0 for success or a filesystem-specific error.
 */
extern errno_t (*_VNOP_ACCESS)(vnode_t, int, vfs_context_t);

/*!
 *  @function VNOP_GETATTR
 *  @abstract Call down to a filesystem to get vnode attributes.
 *  @discussion Supported attributes ("Yes, I am returning this information") are set with VATTR_SET_SUPPORTED.
 *  Which attributes have been requested is checked with VATTR_IS_ACTIVE.  Attributes
 *  are returned with VATTR_RETURN.  It is through VNOP_GETATTR that routines like stat() get their information.
 *  A VNOP_GETATTR() calldown may come without any preceding VNOP_OPEN().
 *  @param vp The vnode whose attributes to get.
 *  @param vap Container for which attributes are requested, which attributes are supported by the filesystem, and attribute values.
 *  @param ctx Context against which to authenticate request for attributes.
 *  @return 0 for success or a filesystem-specific error. VNOP_GETATTR() can return success even if not
 *  all requested attributes were returned; returning an error-value should indicate that something went wrong, rather than that
 *  some attribute is not supported.
 */
extern errno_t (*_VNOP_GETATTR)(vnode_t, struct vnode_attr *, vfs_context_t);

/*!
 *  @function VNOP_SETATTR
 *  @abstract Call down to a filesystem to set vnode attributes.
 *  @discussion Supported attributes ("Yes, I am setting this attribute.") are set with VATTR_SET_SUPPORTED.
 *  Requested attributes are checked with VATTR_IS_ACTIVE.  Attribute values are accessed directly through
 *  structure fields.  VNOP_SETATTR() is the core of the KPI function vnode_setattr(), which is used by chmod(),
 *  chown(), truncate(), and many others.  A VNOP_SETATTR() call may come without any preceding VNOP_OPEN().
 *  @param vp The vnode whose attributes to set.
 *  @param vap Container for which attributes are to be set and their desired values, as well as for the filesystem to
 *  return information about which attributes were successfully set.
 *  @param ctx Context against which to authenticate request for attribute change.
 *  @return 0 for success or a filesystem-specific error.  VNOP_SETATTR() can return success even if not
 *  all requested attributes were set; returning an error-value should indicate that something went wrong, rather than that
 *  some attribute is not supported.
 */
extern errno_t (*_VNOP_SETATTR)(vnode_t, struct vnode_attr *, vfs_context_t);

/*!
 *  @function VNOP_READ
 *  @abstract Call down to a filesystem to read file data.
 *  @discussion VNOP_READ() is where the hard work of of the read() system call happens.  The filesystem may use
 *  the buffer cache, the cluster layer, or an alternative method to get its data; uio routines will be used to see that data
 *  is copied to the correct virtual address in the correct address space and will update its uio argument
 *  to indicate how much data has been moved.
 *  @param vp The vnode to read from.
 *  @param uio Description of request, including file offset, amount of data requested, destination address for data,
 *  and whether that destination is in kernel or user space.
 *  @param ioflag IO flags as defined in vnode.h, e.g. IO_SYNC, IO_NODELOCKED
 *  @param ctx Context against which to authenticate read request.
 *  @return 0 for success or a filesystem-specific error.  VNOP_READ() can return success even if less data was
 *  read than originally requested; returning an error value should indicate that something actually went wrong.
 */
// This function is available to the public KPI (sys/vnode_if.h)
//extern errno_t VNOP_READ(vnode_t vp, struct uio *uio, int ioflag, vfs_context_t ctx);

/*!
 *  @function VNOP_WRITE
 *  @abstract Call down to the filesystem to write file data.
 *  @discussion VNOP_WRITE() is to write() as VNOP_READ() is to read().  The filesystem may use
 *  the buffer cache, the cluster layer, or an alternative method to write its data; uio routines will be used to see that data
 *  is copied to the correct virtual address in the correct address space and will update its uio argument
 *  to indicate how much data has been moved.
 *  @param vp The vnode to write to.
 *  @param uio Description of request, including file offset, amount of data to write, source address for data,
 *  and whether that destination is in kernel or user space.
 *  @param ioflag IO flags as defined in vnode.h, e.g. IO_SYNC, IO_NODELOCKED
 *  @param ctx Context against which to authenticate write request.
 *  @return 0 for success or a filesystem-specific error.  VNOP_WRITE() can return success even if less data was
 *  written than originally requested; returning an error value should indicate that something actually went wrong.
 */
// This function is available to the public KPI (sys/vnode_if.h)
//extern errno_t VNOP_WRITE(vnode_t vp, struct uio *uio, int ioflag, vfs_context_t ctx);

/*!
 *  @function VNOP_IOCTL
 *  @abstract Call down to a filesystem or device driver to execute various control operations on or request data about a file.
 *  @discussion Ioctl controls are typically associated with devices, but they can in fact be passed
 *  down for any file; they are used to implement any of a wide range of controls and information requests.
 *  fcntl() calls VNOP_IOCTL for several commands, and will attempt a VNOP_IOCTL if it is passed an unknown command,
 *  though no copyin or copyout of  arguments can occur in this case--the "arg" must be an integer value.
 *  Filesystems can define their own fcntls using this mechanism.  How ioctl commands are structured
 *  is slightly complicated; see the manual page for ioctl(2).
 *  @param vp The vnode to execute the command on.
 *  @param command Identifier for action to take.
 *  @param data Pointer to data; this can be an integer constant (of 32 bits only) or an address to be read from or written to,
 *  depending on "command."  If it is an address, it is valid and resides in the kernel; callers of VNOP_IOCTL() are
 *  responsible for copying to and from userland.
 *  @param ctx Context against which to authenticate ioctl request.
 *  @return 0 for success or a filesystem-specific error.
 */
// This function is available to the public KPI (sys/vnode_if.h)
//extern errno_t VNOP_IOCTL(vnode_t vp, u_long command, caddr_t data, int fflag, vfs_context_t ctx);

/*!
 *  @function VNOP_SELECT
 *  @abstract Call down to a filesystem or device to check if a file is ready for I/O and request later notification if it is not currently ready.
 *  @discussion In general, regular are always "ready for I/O" and their select vnops simply return "1."
 *  Devices, though, may or may not be read; they keep track of who is selecting on them and send notifications
 *  when they become ready.  xnu provides structures and routines for tracking threads waiting for I/O and waking up
 *  those threads: see selrecord(), selthreadclear(), seltrue(), selwait(), selwakeup(), and the selinfo structure (sys/select.h).
 *  @param vp The vnode to check for I/O readiness.
 *  @param which What kind of I/O is desired: FREAD, FWRITE.
 *  @param fflags Flags from fileglob as seen in fcntl.h, e.g. O_NONBLOCK, O_APPEND.
 *  @param wql Opaque object to pass to selrecord().
 *  @param ctx Context to authenticate for select request.
 *  @return Nonzero indicates that a file is ready for I/O.  0 indicates that the file is not ready for I/O;
 *  there is no way to return an error.  0 should be returned if the device (or file) is not ready for I/O
 *  and the driver (or filesystem) is going to track the request and provide subsequent wakeups.
 *  the device (or filesystem) will provide a wakeup.
 */
extern errno_t (*_VNOP_SELECT)(vnode_t, int, int, void *, vfs_context_t);

/*!
 *  @function VNOP_EXCHANGE
 *  @abstract Call down to a filesystem to atomically exchange the data of two files.
 *  @discussion VNOP_EXCHANGE() is currently only called by the exchangedata() system call.  It will only
 *  be applied to files on the same volume.
 *  @param fvp First vnode.
 *  @param tvp Second vnode.
 *  @param options Unused.
 *  @param ctx Context to authenticate for exchangedata request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_EXCHANGE)(vnode_t, vnode_t, int, vfs_context_t);

/*!
 *  @function VNOP_REVOKE
 *  @abstract Call down to a filesystem to invalidate all open file descriptors for a vnode.
 *  @discussion This function is typically called as part of a TTY revoke, but can also be
 *  used on regular files.  Most filesystems simply use nop_revoke(), which calls vn_revoke(),
 *  as their revoke vnop implementation.
 *  @param vp The vnode to revoke.
 *  @param flags Unused.
 *  @param ctx Context to authenticate for revoke request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_REVOKE)(vnode_t, int, vfs_context_t);

/*!
 *  @function VNOP_MMAP_CHECK
 *  @abstract Check with a filesystem if a file can be mmap-ed.
 *  @discussion VNOP_MMAP_CHECK is used to check with the file system if a
 *  file can be mmap-ed. It will be called before any call to VNOP_MMAP().
 *  @param vp The vnode being mmapped.
 *  @param flags Memory protection: PROT_READ, PROT_WRITE, PROT_EXEC.
 *  @param ctx Context to authenticate for mmap request.
 *  @return 0 for success; EPERM if the operation is not permitted; other
 *  errors (except ENOTSUP) may be returned at the discretion of the file
 *  system.  ENOTSUP will never be returned by VNOP_MMAP_CHECK().
 */
extern errno_t (*_VNOP_MMAP_CHECK)(vnode_t, int, vfs_context_t);

/*!
 *  @function VNOP_MMAP
 *  @abstract Notify a filesystem that a file is being mmap-ed.
 *  @discussion VNOP_MMAP is an advisory calldown to say that the system is mmap-ing a file.
 *  @param vp The vnode being mmapped.
 *  @param flags Memory protection: PROT_READ, PROT_WRITE, PROT_EXEC.
 *  @param ctx Context to authenticate for mmap request.
 *  @return 0 for success; all errors except EPERM are ignored.
 */
extern errno_t (*_VNOP_MMAP)(vnode_t, int, vfs_context_t);

/*!
 *  @function VNOP_MNOMAP
 *  @abstract Inform a filesystem that a file is no longer mapped.
 *  @discussion In general, no action is required of a filesystem for VNOP_MNOMAP.
 *  @param vp The vnode which is no longer mapped.
 *  @param ctx Context to authenticate for mnomap request.
 *  @return Return value is ignored.
 */
extern errno_t (*_VNOP_MNOMAP)(vnode_t, vfs_context_t);

/*!
 *  @function VNOP_FSYNC
 *  @abstract Call down to a filesystem to synchronize a file with on-disk state.
 *  @discussion VNOP_FSYNC is called whenever we need to make sure that a file's data has been
 *  pushed to backing store, for example when recycling; it is also the heart of the fsync() system call.
 *  @param vp The vnode whose data to flush to backing store.
 *  @param ctx Context to authenticate for fsync request.
 *  @return 0 for success, else an error code.
 */
// This function is available to the public KPI
//extern errno_t VNOP_FSYNC(vnode_t vp, int waitfor, vfs_context_t ctx);

/*!
 *  @function VNOP_REMOVE
 *  @abstract Call down to a filesystem to delete a file.
 *  @discussion VNOP_REMOVE is called to remove a file from a filesystem's namespace, for example by unlink().
 *  It can operate on regular files, named pipes, special files, and in some cases on directories.
 *  @param dvp Directory in which to delete a file.
 *  @param vp The file to delete.
 *  @param cnp Filename information.
 *  @param ctx Context to authenticate for fsync request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_REMOVE)(vnode_t, vnode_t, struct componentname *, int, vfs_context_t);

/*!
 *  @function VNOP_LINK
 *  @abstract Call down to a filesystem to create a hardlink to a file.
 *  @discussion See "man 2 link".
 *  @param vp File to link to.
 *  @param dvp Directory in which to create the link.
 *  @param cnp Filename information for new link.
 *  @param ctx Context to authenticate for link request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_LINK)(vnode_t, vnode_t, struct componentname *, vfs_context_t);

/*!
 *  @function VNOP_RENAME
 *  @abstract Call down to a filesystem to rename a file.
 *  @discussion VNOP_RENAME() will only be called with a source and target on the same volume.
 *  @param fdvp Directory in which source file resides.
 *  @param fvp File being renamed.
 *  @param fcnp Name information for source file.
 *  @param tdvp Directory file is being moved to.
 *  @param tvp Existing file with same name as target, should one exist.
 *  @param tcnp Name information for target path.
 *  @param ctx Context to authenticate for rename request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_RENAME)(vnode_t, vnode_t, struct componentname *, vnode_t, vnode_t, struct componentname *, vfs_context_t);

/*!
 *  @function VNOP_RENAMEX
 *  @abstract Call down to a filesystem to rename a file.
 *  @discussion VNOP_RENAMEX() will only be called with a source and target on the same volume.
 *  @param fdvp Directory in which source file resides.
 *  @param fvp File being renamed.
 *  @param fcnp Name information for source file.
 *  @param tdvp Directory file is being moved to.
 *  @param tvp Existing file with same name as target, should one exist.
 *  @param tcnp Name information for target path.
 *  @param flags Control certain rename semantics.
 *  @param ctx Context to authenticate for rename request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_RENAMEX)(vnode_t, vnode_t, struct componentname *, vnode_t, vnode_t, struct componentname *, vfs_rename_flags_t, vfs_context_t);

/*!
 *  @function VNOP_MKDIR
 *  @abstract Call down to a filesystem to create a directory.
 *  @discussion The newly created directory should be returned with an iocount which will be dropped by the caller.
 *  @param dvp Directory in which to create new directory.
 *  @param vpp Destination for pointer to new directory's vnode.
 *  @param cnp Name information for new directory.
 *  @param vap Attributes for new directory.
 *  @param ctx Context to authenticate for mkdir request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_MKDIR)(vnode_t, vnode_t *, struct componentname *, struct vnode_attr *, vfs_context_t);

/*!
 *  @function VNOP_RMDIR
 *  @abstract Call down to a filesystem to delete a directory.
 *  @param dvp Parent of directory to be removed.
 *  @param vp Directory to remove.
 *  @param cnp Name information for directory to be deleted.
 *  @param ctx Context to authenticate for rmdir request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_RMDIR)(vnode_t, vnode_t, struct componentname *, vfs_context_t);

/*!
 *  @function VNOP_SYMLINK
 *  @abstract Call down to a filesystem to create a symbolic link.
 *  @param If VNOP_SYMLINK() is successful, the new file should be returned with an iocount which will
 *  be dropped by the caller.  VFS does not ensure that the target path will have a length shorter
 *  than the max symlink length for the filesystem.
 *  @param dvp Parent directory for new symlink file.
 *  @param vpp
 *  @param cnp Name information for new symlink.
 *  @param vap Attributes for symlink.
 *  @param target Path for symlink to store; for "ln -s /var/vardir linktovardir", "target" would be "/var/vardir"
 *  @param ctx Context to authenticate for symlink request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_SYMLINK)(vnode_t, vnode_t *, struct componentname *, struct vnode_attr *, char *, vfs_context_t);

/*!
 *  @function VNOP_READDIR
 *  @abstract Call down to a filesystem to enumerate directory entries.
 *  @discussion VNOP_READDIR() packs a buffer with "struct dirent" directory entry representations as described
 *  by the "getdirentries" manual page.
 *  @param vp Directory to enumerate.
 *  @param uio Destination information for resulting direntries.
 *  @param flags VNODE_READDIR_EXTENDED, VNODE_READDIR_REQSEEKOFF, VNODE_READDIR_SEEKOFF32: Apple-internal flags.
 *  @param eofflag Should be set to 1 if the end of the directory has been reached.
 *  @param numdirent Should be set to number of entries written into buffer.
 *  @param ctx Context to authenticate for readdir request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_READDIR)(vnode_t, struct uio *, int, int *, int *, vfs_context_t);

/*!
 *  @function VNOP_READDIRATTR
 *  @abstract Call down to get file attributes for many files in a directory at once.
 *  @discussion VNOP_READDIRATTR() packs a buffer  with file attributes, as if the results of many "getattrlist" calls.
 *  @param vp Directory in which to enumerate entries' attributes.
 *  @param alist Which attributes are wanted for each directory entry.
 *  @param uio Destination information for resulting attributes.
 *  @param maxcount Maximum count of files to get attributes for.
 *  @param options FSOPT_NOFOLLOW: do not follow symbolic links. FSOPT_NOINMEMUPDATE: do not use data which have been
 *  updated since an inode was loaded into memory.
 *  @param newstate The "newstate" should be set to a value which changes if the contents of a directory change
 *  through an addition or deletion but stays the same otherwise.
 *  @param eofflag Should be set to 1 if the end of the directory has been reached.
 *  @param actualcount Should be set to number of files whose attributes were  written into buffer.
 *  @param ctx Context to authenticate for readdirattr request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_READDIRATTR)(vnode_t, struct attrlist *, struct uio *, uint32_t, uint32_t, uint32_t *, int *, uint32_t *, vfs_context_t);

/*!
 *  @function VNOP_GETATTRLISTBULK
 *  @abstract Call down to get file attributes for many files in a directory at once.
 *  @discussion VNOP_GETATTRLISTBULK() packs a buffer  with file attributes, as if the results of many "getattrlist" calls.
 *  @param vp Directory in which to enumerate entries' attributes.
 *  @param alist Which attributes are wanted for each directory entry.
 *  @param uio Destination information for resulting attributes.
 *  @param vap initialised vnode_attr structure pointer. This structure also has memory allocated (MAXPATHLEN bytes) and assigned to the va_name field for filesystems to use.
 *  @param private reserved for future use.
 *  @param options
 *  @param eofflag Should be set to 1 if the end of the directory has been reached.
 *  @param actualcount Should be set to number of files whose attributes were  written into buffer.
 *  @param ctx Context to authenticate for getattrlistbulk request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_GETATTRLISTBULK)(vnode_t, struct attrlist *, struct vnode_attr *, uio_t, void *, uint64_t, int32_t *, int32_t *, vfs_context_t);

/*!
 *  @function VNOP_READLINK
 *  @abstract Call down to a filesystem to get the pathname represented by a symbolic link.
 *  @discussion VNOP_READLINK() gets the path stored in a symbolic link; it is called by namei() and the readlink() system call.
 *  @param vp Symbolic link to read from.
 *  @param uio Destination information for link path.
 *  @param ctx Context to authenticate for readlink request.
 *  @return 0 for success, else an error code.
 */
extern errno_t (*_VNOP_READLINK)(vnode_t, struct uio *, vfs_context_t);

/*!
 *  @function VNOP_INACTIVE
 *  @abstract Notify a filesystem that the last usecount (persistent reference) on a vnode has been dropped.
 *  @discussion VNOP_INACTVE() gives a filesystem a chance to aggressively release resources assocated with a vnode, perhaps
 *  even to call vnode_recycle(), but no action is prescribed; it is acceptable for VNOP_INACTIVE to be a no-op and
 *  to defer all reclamation until VNOP_RECLAIM().
 *  VNOP_INACTVE() will not be called on a vnode if no persistent reference is ever taken; an
 *  important example is a stat(), which takes an iocount, reads its data, and drops that iocount.
 *  @param vp The vnode which is now inactive.
 *  @param ctx Context to authenticate for inactive message.
 *  @return 0 for success, else an error code, but return value is currently ignored.
 */
extern errno_t (*_VNOP_INACTIVE)(vnode_t, vfs_context_t);

/*!
 *  @function VNOP_RECLAIM
 *  @abstract Release filesystem-internal resources for a vnode.
 *  @discussion VNOP_RECLAIM() is called as part of the process of recycling a vnode.  During
 *  a reclaim routine, a filesystem should remove a vnode from its hash and deallocate any resources
 *  allocated to that vnode.  VFS guarantees that when VNOP_RECLAIM() is called, there are no more
 *  iocount references on a vnode (though there may still be usecount references--these are invalidated
 *  by the reclaim) and that no more will be granted.  This means in practice that there will be no
 *  filesystem calls on the vnode being reclaimed until the reclaim has finished and the vnode has
 *  been reused.
 *  @param vp The vnode to reclaim.
 *  @param ctx Context to authenticate for reclaim.
 *  @return 0 for success, or an error code.  A nonzero return value results in a panic.
 */
extern errno_t (*_VNOP_RECLAIM)(vnode_t, vfs_context_t);

/*!
 *  @function VNOP_PATHCONF
 *  @abstract Query a filesystem for path properties.
 *  @param vp The vnode whose filesystem to query.
 *  @param name Which property to request: see unistd.h.  For example: _PC_CASE_SENSITIVE (is
 *  a filesystem case-sensitive?).  Only one property can be requested at a time.
 *  @param retval Destination for value of property.
 *  @param ctx Context to authenticate for pathconf request.
 *  @return 0 for success, or an error code.
 */
extern errno_t (*_VNOP_PATHCONF)(vnode_t, int, int32_t *, vfs_context_t);

/*!
 *  @function VNOP_ADVLOCK
 *  @abstract Aquire or release and advisory lock on a vnode.
 *  @discussion Advisory locking is somewhat complicated.  VNOP_ADVLOCK is overloaded for
 *  both flock() and POSIX advisory locking usage, though not all filesystems support both (or any).  VFS
 *  provides an advisory locking mechanism for filesystems which can take advantage of it; vfs_setlocklocal()
 *  marks a filesystem as using VFS advisory locking support.
 *  @param vp The vnode to lock or unlock.
 *  @param id Identifier for lock holder: ignored by most filesystems.
 *  @param op Which locking operation: F_SETLK: set locking information about a region.
 *  F_GETLK: get locking information about the specified region.  F_UNLCK: Unlock a region.
 *  @param fl Description of file region to lock. l_whence is as with "lseek."
 *  Includes a type: F_RDLCK (shared lock), F_UNLCK (unlock) , and F_WRLCK (exclusive lock).
 *  @param flags F_FLOCK: use flock() semantics. F_POSIX: use POSIX semantics.  F_WAIT: sleep if necessary.
 *  F_PROV: Non-coelesced provisional lock (unused in xnu).
 *  @param ctx Context to authenticate for advisory locking request.
 *  @param timeout Timespec for timeout in case of F_SETLKWTIMEOUT.
 *  @return 0 for success, or an error code.
 */
extern errno_t (*_VNOP_ADVLOCK)(vnode_t, caddr_t, int, struct flock *, int, vfs_context_t, struct timespec *);

/*!
 *  @function VNOP_ALLOCATE
 *  @abstract Pre-allocate space for a file.
 *  @discussion VNOP_ALLOCATE() changes the amount of backing store set aside to
 *  a file.  It can be used to either shrink or grow a file.  If the file shrinks,
 *  its ubc size will be modified accordingly, but if it grows, then the ubc size is unchanged;
 *  space is set aside without being actively used by the file.  VNOP_ALLOCATE() is currently only
 *  called as part of the F_PREALLOCATE fcntl.
 *  @param vp The vnode for which to preallocate space.
 *  @param length Desired preallocated file length.
 *  @param flags
 *  PREALLOCATE:     preallocate allocation blocks.
 *  ALLOCATECONTIG:  allocate contigious space.
 *  ALLOCATEALL:     allocate all requested space or no space at all.
 *  FREEREMAINDER:   deallocate allocated but unfilled blocks.
 *  ALLOCATEFROMPEOF: allocate from the physical eof.
 *  ALLOCATEFROMVOL:  allocate from the volume offset.
 *  @param bytesallocated  Additional bytes set aside for file. Set to 0 if none are allocated
 *  OR if the file is contracted.
 *  @param offset Hint for where to find free blocks.
 *  @param ctx Context to authenticate for allocation request.
 *  @return 0 for success, or an error code.
 */
extern errno_t (*_VNOP_ALLOCATE)(vnode_t, off_t, u_int32_t, off_t *, off_t, vfs_context_t);

/*!
 *  @function VNOP_PAGEIN
 *  @abstract Pull file data into memory.
 *  @discussion VNOP_PAGEIN() is called by when a process faults on data mapped from a file or
 *  when madvise() demands pre-fetching.  It is conceptually somewhat similar to VNOP_READ().  Filesystems
 *  are typically expected to call cluster_pagein() to handle the labor of mapping and committing the UPL.
 *  @param vp The vnode for which to page in data.
 *  @param pl UPL describing pages needing to be paged in.
 *  @param pl_offset Offset in UPL at which to start placing data.
 *  @param f_offset Offset in file of data needing to be paged in.
 *  @param size Amount of data to page in (in bytes).
 *  @param flags UPL-style flags: UPL_IOSYNC, UPL_NOCOMMIT, UPL_NORDAHEAD, UPL_VNODE_PAGER, UPL_MSYNC.
 *  Filesystems should generally leave it to the cluster layer to handle these flags. See the
 *  memory_object_types.h header in the kernel framework if interested.
 *  @param ctx Context to authenticate for pagein request.
 *  @return 0 for success, or an error code.
 */
extern errno_t (*_VNOP_PAGEIN)(vnode_t, upl_t, upl_offset_t, off_t, size_t, int, vfs_context_t);

/*!
 *  @function VNOP_PAGEOUT
 *  @abstract Write data from a mapped file back to disk.
 *  @discussion VNOP_PAGEOUT() is called when data from a mapped file needs to be flushed to disk, either
 *  because of an msync() call or due to memory pressure.  Filesystems are for the most part expected to
 *  just call cluster_pageout().   However, if they opt into the VFC_VFSVNOP_PAGEOUTV2 flag, then
 *  they will be responsible for creating their own UPLs.
 *  @param vp The vnode for which to page out data.
 *  @param pl UPL describing pages needed to be paged out.  If UPL is NULL, then it means the filesystem
 *  has opted into VFC_VFSVNOP_PAGEOUTV2 semantics, which means that it will create and operate on its own UPLs
 *  as opposed to relying on the one passed down into the filesystem.  This means that the filesystem must be
 *  responsible for N cluster_pageout calls for N dirty ranges in the UPL.
 *  @param pl_offset Offset in UPL from which to start paging out data.  Under the new VFC_VFSVNOP_PAGEOUTV2
 *  semantics, this is the offset in the range specified that must be paged out if the associated page is dirty.
 *  @param f_offset Offset in file of data needing to be paged out.    Under the new VFC_VFSVNOP_PAGEOUTV2
 *  semantics, this represents the offset in the file where we should start looking for dirty pages.
 *  @param size Amount of data to page out (in bytes).   Under VFC_VFSVNOP_PAGEOUTV2, this represents
 *  the size of the range to be considered.  The fileystem is free to extend or shrink the specified range
 *  to better fit its blocking model as long as the page at 'pl_offset' is included.
 *  @param flags UPL-style flags: UPL_IOSYNC, UPL_NOCOMMIT, UPL_NORDAHEAD, UPL_VNODE_PAGER, UPL_MSYNC.
 *  Filesystems should generally leave it to the cluster layer to handle these flags. See the
 *  memory_object_types.h header in the kernel framework if interested.
 *  @param ctx Context to authenticate for pageout request.
 *  @return 0 for success, or an error code.
 */
extern errno_t (*_VNOP_PAGEOUT)(vnode_t, upl_t, upl_offset_t, off_t, size_t, int, vfs_context_t);

/*
 *  @function VNOP_SEARCHFS
 *  @abstract Search a filesystem quickly for files or directories that match the passed-in search criteria.
 *  @discussion VNOP_SEARCHFS is a getattrlist-based system call which is implemented almost entirely inside
 *  supported filesystems.  Callers provide a set of criteria to match against, and the filesystem is responsible
 *  for finding all files or directories that match the criteria.  Once these files or directories are found,
 *  the user-requested attributes of these files is provided as output.  The set of searchable attributes is a
 *  subset of the getattrlist  attributes.  For example, ATTR_CMN_UUID is not a valid searchable attribute as of
 *  10.6.  A common usage scenario could be to request all files whose mod dates is greater than time X, less than
 *  time Y, and provide the inode ID and filename of the matching objects as output.
 *  @param vp The vnode representing the mountpoint of the filesystem to be searched.
 *  @param a_searchparams1 If one-argument search criteria is requested, the search criteria would go here. However,
 *  some search criteria, like ATTR_CMN_MODTIME, can be bounded.  The user could request files modified between time X
 *  and time Y.  In this case, the lower bound goes in a_searchparams1.
 *  @param a_searchparams2 If two-argument search criteria is requested, the upper bound goes in here.
 *  @param a_searchattrs Contains the getattrlist-style attribute bits which are requested by the current search.
 *  @param a_maxmatches The maximum number of matches to return in a single system call.
 *  @param a_timelimit The suggested maximum amount of time we can spend in the kernel to service this system call.
 *  Filesystems should use this as a guide only, and set their own internal maximum time to avoid denial of service.
 *  @param a_returnattrs The getattrlist-style attributes to return for items in the filesystem that match the search
 *  criteria above.
 *  @param a_scriptcode Currently ignored.
 *  @param a_uio The uio in which to write out the search matches.
 *  @param a_searchstate Sometimes searches cannot be completed in a single system call.  In this case, we provide
 *  an identifier back to the user which indicates where to resume a previously-started search.  This is an opaque structure
 *  used by the filesystem to identify where to resume said search.
 *  @param a_context The context in which to perform the filesystem search.
 *  @return 0 on success, EAGAIN for searches which could not be completed in 1 call, and other ERRNOS as needed.
 */
extern errno_t (*_VNOP_SEARCHFS)(vnode_t, void *, void *, struct attrlist *, uint32_t, struct timeval *, struct attrlist *, uint32_t *, uint32_t, uint32_t, struct uio *, struct searchstate *, vfs_context_t);

extern errno_t (*_VNOP_COPYFILE)(vnode_t, vnode_t, vnode_t, struct componentname *, int, int, vfs_context_t);

/*!
 *  @function VNOP_CLONEFILE
 *  @abstract Call down to a filesystem to clone a filesystem object (regular file, directory or symbolic link.)
 *  @discussion If file creation succeeds, "vpp" should be returned with an iocount to be dropped by the caller.
 *  @param dvp Directory in which to clone object.
 *  @param vpp Destination for vnode for newly cloned object.
 *  @param cnp Description of name of object to clone.
 *  @param vap File creation properties, as seen in vnode_getattr().  Manipulated with VATTR_ISACTIVE, VATTR_RETURN,
 *  VATTR_SET_SUPPORTED, and so forth. All attributes not set here should either be copied
 *  from the source object
 *  or set to values which are used for creating new filesystem objects
 *  @param ctx Context against which to authenticate file creation.
 *  @return 0 for success or a filesystem-specific error.
 */
extern errno_t (*_VNOP_CLONEFILE)(vnode_t, vnode_t, vnode_t *, struct componentname *, struct vnode_attr *, uint32_t, vfs_context_t);

#endif /* procfs_internal_h */
