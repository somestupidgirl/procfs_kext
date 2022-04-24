#ifndef _fd_h
#define _fd_h

#include <sys/types.h>

#define NDFILE          20
#define NDSLOTTYPE      u_long
#define NDSLOTSIZE      sizeof(NDSLOTTYPE)
#define NDENTRIES       (NDSLOTSIZE * CHAR_BIT)
#define NDSLOT(x)       ((x) / NDENTRIES)
#define NDBIT(x)        ((NDSLOTTYPE)1 << ((x) % NDENTRIES))
#define NDSLOTS(x)      (((x) + NDENTRIES - 1) / NDENTRIES)

#endif /* _fd_h */
