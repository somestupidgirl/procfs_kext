/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_iokit.h
 *
 * Bridge between the C VFS code and the C++ IOKit helper (procfs_iokit.cpp),
 * which enumerates block devices for the partitions node. Included from both
 * languages.
 */
#ifndef _FS_PROCFS_PROCFS_IOKIT_H_
#define _FS_PROCFS_PROCFS_IOKIT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One block device (IOMedia): a whole disk or a partition. */
struct procfs_partition {
    uint32_t major;         /* BSD major device number */
    uint32_t minor;         /* BSD minor device number */
    uint64_t size;          /* size in bytes            */
    char     name[64];      /* BSD name, e.g. "disk0" / "disk0s1" */
};

/*
 * Enumerate block devices via IOKit (matching the "IOMedia" class), filling up
 * to `max` entries into `out` and setting *count. Returns 0 on success or an
 * errno on failure (the caller then falls back to the mounted-filesystem list).
 */
int procfs_iokit_get_partitions(struct procfs_partition *out, int max, int *count);

#ifdef __cplusplus
}
#endif

#endif /* _FS_PROCFS_PROCFS_IOKIT_H_ */
