/*
 * Various macros and definitions pulled from
 * FreeBSD and NetBSD.
 */
#ifndef _procfs_helpers_h
#define _procfs_helpers_h

#include <stdint.h>
#include <i386/limits.h>
#include <mach/i386/vm_param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/types.h>

#pragma mark -
#pragma mark NetBSD/mouse-BSD macros and definitions

/*
 * Buffer size definition from NetBSD's miscfs/procfs/procfs_linux.c
 */
#define LBFSZ           (8 * 1024)

/*
 * Conversion macros from NetBSD's miscfs/procfs/procfs_linux.c.
 * Similar to P2B and P2K in FreeBSD.
 */
#define PGTOB(p)        ((unsigned long)(p) << PAGE_SHIFT)
#define PGTOKB(p)       ((unsigned long)(p) << (PAGE_SHIFT - 10))

/*
 * The following macro is used to remove const cast-away warnings
 * from gcc -Wcast-qual; it should be used with caution because it
 * can hide valid errors; in particular most valid uses are in
 * situations where the API requires it, not to cast away string
 * constants. We don't use *intptr_t on purpose here and we are
 * explicit about unsigned long so that we don't have additional
 * dependencies.
 */
#define __UNCONST(a)    ((void *)(unsigned long)(const void *)(a))

#pragma mark -
#pragma mark FreeBSD Macros and definitions

/*
 * File descriptor-related macros from FreeBSD
 */
#define NDFILE          20
#define NDSLOTTYPE      u_long
#define NDSLOTSIZE      sizeof(NDSLOTTYPE)
#define NDENTRIES       (NDSLOTSIZE * CHAR_BIT)
#define NDSLOT(x)       ((x) / NDENTRIES)
#define NDBIT(x)        ((NDSLOTTYPE)1 << ((x) % NDENTRIES))
#define NDSLOTS(x)      (((x) + NDENTRIES - 1) / NDENTRIES)

/*
 * Various conversion macros from FreeBSD
 */
#define B2K(x)          ((x) >> 10)														/* bytes to kbytes */
#define B2P(x)          ((x) >> PAGE_SHIFT)												/* bytes to pages */
#define P2B(x)          ((x) << PAGE_SHIFT)												/* pages to bytes */
#define P2K(x)          ((x) << (PAGE_SHIFT - 10))										/* pages to kbytes */
#define TV2J(x)	        ((x)->tv_sec * 100UL + (x)->tv_usec / 10000)

/*
 * calculates the length of an array in number of items.
 */
#define nitems(x)       ((int)sizeof((x)) / (int)sizeof((x)[0]))

/*
 * BSD malloc() and free()
 */
#define malloc(size, type, flags)    _MALLOC(size, type, flags)
#define free(addr, type)             _FREE(addr, type)

#pragma mark -
#pragma mark XNU macros and definitions

/*
 * Used in osfmk/i386/cpuid.c
 */
#define quad(hi, lo)    (((uint64_t)(hi)) << 32 | (lo))

/*
 * Multipliers used to encode 1*K .. 64*M in a 16 bit size field
 */
#define K               (1)
#define M               (1024)

#pragma mark -
#pragma mark Static inline functions

/*
 * Misc conversion functions.
 */
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
