/*
 * Macros from FreeBSD sys/kern/kern_descrip.c
 */

#ifndef _kern_descrip_h
#define _kern_descrip_h

#include <sys/filedesc.h>
#include <sys/types.h>

/*
 * Each process has:
 *
 * - An array of open file descriptors (fd_ofiles)
 * - An array of file flags (fd_ofileflags)
 * - A bitmap recording which descriptors are in use (fd_map)
 *
 * A process starts out with NDFILE descriptors.  The value of NDFILE has
 * been selected based the historical limit of 20 open files, and an
 * assumption that the majority of processes, especially short-lived
 * processes like shells, will never need more.
 *
 * If this initial allocation is exhausted, a larger descriptor table and
 * map are allocated dynamically, and the pointers in the process's struct
 * filedesc are updated to point to those.  This is repeated every time
 * the process runs out of file descriptors (provided it hasn't hit its
 * resource limit).
 *
 * Since threads may hold references to individual descriptor table
 * entries, the tables are never freed.  Instead, they are placed on a
 * linked list and freed only when the struct filedesc is released.
 */
#define NDSLOTTYPE      u_long
#define NDSLOTSIZE      sizeof(NDSLOTTYPE)
#define NDENTRIES       (NDSLOTSIZE * CHAR_BIT)
#define NDSLOT(x)       ((x) / NDENTRIES)
#define NDBIT(x)        ((NDSLOTTYPE)1 << ((x) % NDENTRIES))
#define NDSLOTS(x)      (((x) + NDENTRIES - 1) / NDENTRIES)

#endif /* _kern_descrip_h */
