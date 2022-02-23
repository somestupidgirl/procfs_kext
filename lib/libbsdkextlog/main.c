/*
 * Created 180826 lynnl
 */

#include <mach/mach_types.h>
#include <libkern/libkern.h>

extern kern_return_t _start(kmod_info_t *, void *);
extern kern_return_t _stop(kmod_info_t *, void *);

#if  0
kern_return_t
_start(kmod_info_t *, void *)
{
    kern_return_t ret = KERN_SUCCESS;

    log_sysctl_register();

    ret = kauth_register();
    if (ret != KERN_SUCCESS) {
        goto out_error;
    }

    ret = log_kctl_register();
    if (ret != KERN_SUCCESS) {
        kauth_deregister();
        goto out_error;
    }

    if (ret == KERN_SUCCESS) {
        goto out_exit;
    }

out_exit
    return ret;

out_error:
    ret = KERN_FAILURE;
    goto out_exit;
}


#if 0
kern_return_t
_stop(kmod_info_t *, void *)
    ret = log_kctl_deregister();
    if (ret != KERN_SUCCESS) {
        LOG_ERR("log_kctl_deregister() failed  errno: %d \n", ret);
        goto out_error;
    }

    if (ret == KERN_SUCCESS) {
        goto out_exit;
    }

out_exit
    return ret;

out_error:
    ret = KERN_FAILURE;
    goto out_exit;
}
#endif
#endif

/* If you intended to write a kext library  NULLify _realmain and _antimain */
__private_extern__ kmod_start_func_t *_realmain = NULL;
__private_extern__ kmod_stop_func_t *_antimain = NULL;

__private_extern__ int _kext_apple_cc = __APPLE_CC__;
