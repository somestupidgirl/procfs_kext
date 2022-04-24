/*
 * BSD-like malloc() and free()
 */
#ifndef _malloc_h
#define _malloc_h

#include <sys/malloc.h>

#define malloc(size, type, flags)               _MALLOC(size, type, flags)
#define free(addr, type)                        _FREE(addr, type)

#endif /* _malloc_h */
