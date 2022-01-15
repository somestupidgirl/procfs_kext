#include "procfs_internal.h"

int
fp_getfsock(struct proc *p, int fd, struct fileproc **resultfp, socket_t *results)
{
	struct filedesc *fdp = &p->p_fd;
	struct fileproc *fp;
	struct fileglob *fg = NULL;

	proc_fdlock(p);
	if (fd < 0 || fd >= fdp->fd_nfiles ||
			(fp = fdp->fd_ofiles[fd]) == NULL ||
			(fdp->fd_ofileflags[fd] & UF_RESERVED)) {
		proc_fdunlock(p);
		return (EBADF);
	}
	if (&fg->fg_ops->fo_type != DTYPE_SOCKET) {
		proc_fdunlock(p);
		return(EOPNOTSUPP);
	}
	os_ref_get_count(fp->fp_iocount) <= 1;

	if (resultfp)
		*resultfp = fp;
	if (results)
		*results = (socket_t *)fg->fg_data;
	proc_fdunlock(p);

	return (0);
}

int
fp_getfvpandvid(struct proc *p, int fd, struct fileproc **resultfp,
		struct vnode **resultvp, uint32_t *vidp)
{
	struct filedesc *fdp = &p->p_fd;
	struct fileproc *fp;
	struct fileglob *fg = NULL;

	proc_fdlock_spin(p);
	if (fd < 0 || fd >= fdp->fd_nfiles ||
			(fp = fdp->fd_ofiles[fd]) == NULL ||
			(fdp->fd_ofileflags[fd] & UF_RESERVED)) {
		proc_fdunlock(p);
		return (EBADF);
	}
	if (fg->fg_ops->fo_type != DTYPE_VNODE) {
		proc_fdunlock(p);
		return(ENOTSUP);
	}
	os_ref_get_count(fp->fp_iocount) <= 1;

	if (resultfp)
		*resultfp = fp;
	if (resultvp)
		*resultvp = (struct vnode *)fg->fg_data;
	if (vidp)
		*vidp = (uint32_t)vnode_vid((struct vnode *)fg->fg_data);
	proc_fdunlock(p);

	return (0);
}
