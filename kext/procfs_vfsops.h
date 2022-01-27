#ifndef procfs_vfsops_h
#define procfs_vfsops_h

#define VFC_VFSNOMACLABEL   0x1000

extern struct vfsops procfs_vfsops;

int procfs_init(struct vfsconf *vfsconf);
void procfs_fini(void);

#endif /* procfs_vfsops_h */