#ifndef procfs_vnops_h
#define procfs_vnops_h

extern const struct vnodeopv_desc *procfs_vnopv_desc_list[];
extern int(**procfs_vnodeop_p)(void *);

#endif /* procfs_vnops_h */
