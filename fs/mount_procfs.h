#ifndef mount_procfs_h
#define mount_procfs_h

// File system type name.
#define PROCFS_FSNAME "procfs"

// Mount option flags.
// Do not apply process permissions to the pid entries in /proc.
#define PROCFS_MOPT_NOPROCPERMS (1 << 0)

/*
 * The procfs mount structure, created by mount_procfs
 * and passed to the kernel by the mount(2) system call.
 */
typedef struct procfs_mount_args {
    int mnt_options;      // The procfs mount options.
} procfs_mount_args_t;

#endif /* mount_procfs_h */