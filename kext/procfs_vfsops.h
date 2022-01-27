#ifndef procfs_vfsops_h
#define procfs_vfsops_h

int procfs_init(struct vfsconf *vfsconf);
void procfs_fini(void);

#endif /* procfs_vfsops_h */