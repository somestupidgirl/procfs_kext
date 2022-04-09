/*
 * Various macros and definitions pulled from
 * FreeBSD and NetBSD.
 */
#ifndef _procfs_helpers_h
#define _procfs_helpers_h

#include <stdint.h>
#include <mach/i386/vm_param.h>

#include "symbols.h"

#pragma mark -
#pragma mark Definitions

#define LBFSZ           (8 * 1024)


#pragma mark -
#pragma mark Macros

/*
 * Various conversion macros
 */
#define T2J(x)         ((long)(((x) * 100ULL) / (_stathz ? _stathz : _hz)))				/* ticks to jiffies */
#define T2CS(x)        ((unsigned long)(((x) * 100ULL) / (_stathz ? _stathz : _hz)))	/* ticks to centiseconds */
#define T2S(x)         ((x) / (_stathz ? _stathz : _hz))								/* ticks to seconds */
#define B2K(x)         ((x) >> 10)														/* bytes to kbytes */
#define B2P(x)         ((x) >> PAGE_SHIFT)												/* bytes to pages */
#define P2B(x)         ((x) << PAGE_SHIFT)												/* pages to bytes */
#define P2K(x)         ((x) << (PAGE_SHIFT - 10))										/* pages to kbytes */
#define TV2J(x)	       ((x)->tv_sec * 100UL + (x)->tv_usec / 10000)

#define quad(hi,lo)    (((uint64_t)(hi)) << 32 | (lo))

/*
 * calculates the length of an array in number of items.
 */
#define nitems(x)      ((int)sizeof((x)) / (int)sizeof((x)[0]))

/*
 * BSD-style malloc
 */
#define malloc(x,y,z)   _MALLOC(x,y,z)
#define free(x,y)       _FREE(x,y)

#pragma mark -
#pragma mark Inline Helper Functions

static inline int
isupper(char c)
{
    return c >= 'A' && c <= 'Z';
}

static inline int
islower(char c)
{
    return c >= 'a' && c <= 'z';
}

static inline int
isspace(char c)
{
    return c == ' ' || (c >= 0x9 && c <= 0xd);
}

static inline int
isdigit(char c)
{
    return c >= '0' && c <= '9';
}

static inline int
isxdigit(char c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline int
isalpha(char c)
{
    return isupper(c) || islower(c);
}

static inline int
isalnum(char c)
{
    return isalpha(c) || isdigit(c);
}

static inline int
toupper(char c)
{
    return islower(c) ? c - 'a' + 'A' : c;
}

static inline int
tolower(char c)
{
    return isupper(c) ? c - 'A' + 'a' : c;
}

#endif /* _procfs_helpers_h */
