#include "procfs_internal.h"

/*
 * fp_getfvpandvid
 *
 * Description:	Get fileproc, vnode pointer, and vid for a given fd from the
 *		per process open file table of the specified process, and if
 *		successful, increment the f_iocount
 *
 * Parameters:	p				Process in which fd lives
 *		fd				fd to get information for
 *		resultfp			Pointer to result fileproc
 *						pointer area, or 0 if none
 *		resultvp			Pointer to result vnode pointer
 *						area, or 0 if none
 *		vidp				Pointer to resuld vid area
 *
 * Returns:	0				Success
 *		EBADF				Bad file descriptor
 *		ENOTSUP				fd does not refer to a vnode
 *
 * Implicit returns:
 *		*resultfp (modified)		Fileproc pointer
 *		*resultvp (modified)		vnode pointer
 *		*vidp				vid value
 *
 * Notes:	The resultfp and resultvp fields are optional, and may be
 *		independently specified as NULL to skip returning information
 *
 * Locks:	Internally takes and releases proc_fdlock
 */
// bsd/kern/kern_descrip.c
int
fp_getfvpandvid(proc_t p, int fd, struct fileproc **resultfp, struct vnode **resultvp, uint32_t *vidp)
{
#if 0
	struct filedesc *fdp = p->p_fd;
	struct fileproc *fp;

	proc_fdlock_spin(p);
	if (fd < 0 || fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL ||
	    (fdp->fd_ofileflags[fd] & UF_RESERVED)) {
		proc_fdunlock(p);
		return EBADF;
	}
	if (fp->f_type != DTYPE_VNODE) {
		proc_fdunlock(p);
		return ENOTSUP;
	}
	os_ref_retain_locked(&fp->f_iocount);

	if (resultfp) {
		*resultfp = fp;
	}
	if (resultvp) {
		*resultvp = (struct vnode *)fp->f_data;
	}
	if (vidp) {
		*vidp = (uint32_t)vnode_vid((struct vnode *)fp->f_data);
	}
	proc_fdunlock(p);
#endif
	return 0;
}

/*
 * fp_getfsock
 *
 * Description:	Get fileproc and socket pointer for a given fd from the
 *		per process open file table of the specified process, and if
 *		successful, increment the f_iocount
 *
 * Parameters:	p				Process in which fd lives
 *		fd				fd to get information for
 *		resultfp			Pointer to result fileproc
 *						pointer area, or 0 if none
 *		results				Pointer to result socket
 *						pointer area, or 0 if none
 *
 * Returns:	EBADF			The file descriptor is invalid
 *		EOPNOTSUPP		The file descriptor is not a socket
 *		0			Success
 *
 * Implicit returns:
 *		*resultfp (modified)		Fileproc pointer
 *		*results (modified)		socket pointer
 *
 * Notes:	EOPNOTSUPP should probably be ENOTSOCK; this function is only
 *		ever called from accept1().
 */
// bsd/kern/kern_descrip.c
int fp_getfsock(proc_t p, int fd, struct fileproc **resultfp, struct socket **results)
{
#if 0
	struct filedesc *fdp = p->p_fd;
	struct fileproc *fp;

	proc_fdlock_spin(p);
	if (fd < 0 || fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL ||
	    (fdp->fd_ofileflags[fd] & UF_RESERVED)) {
		proc_fdunlock(p);
		return EBADF;
	}
	if (fp->f_type != DTYPE_SOCKET) {
		proc_fdunlock(p);
		return EOPNOTSUPP;
	}
	os_ref_retain_locked(&fp->f_iocount);

	if (resultfp) {
		*resultfp = fp;
	}
	if (results) {
		*results = (struct socket *)fp->f_data;
	}
	proc_fdunlock(p);
#endif
	return 0;
}
