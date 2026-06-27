/*
 * Created 180826 lynnl
 */

#include <mach/mach_types.h>
#include <libkern/libkern.h>

extern kern_return_t _start(kmod_info_t *, void *);
extern kern_return_t _stop(kmod_info_t *, void *);

/* If you intended to write a kext library  NULLify _realmain and _antimain */
__private_extern__ kmod_start_func_t *_realmain = NULL;
__private_extern__ kmod_stop_func_t *_antimain = NULL;

__private_extern__ int _kext_apple_cc = __APPLE_CC__;