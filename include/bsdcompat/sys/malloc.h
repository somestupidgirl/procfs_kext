/*
 * BSD-like malloc() and free()
 */
#ifndef _malloc_h
#define _malloc_h

#include <sys/malloc.h>

/*
 * The latest <sys/malloc.h> only prototypes _MALLOC()/_FREE() in the
 * !XNU_KERNEL_PRIVATE branch (they are deprecated in favour of kalloc_type()),
 * so they are not visible in this kernel-private build. The symbols are still
 * exported BSD KPI, so declare them here for procfs's BSD-style wrappers.
 */
extern void *_MALLOC(size_t size, int type, int flags);
extern void  _FREE(void *addr, int type);

#define malloc(size, type, flags)               _MALLOC(size, type, flags)
#define free(addr, type)                        _FREE(addr, type)

#endif /* _malloc_h */
