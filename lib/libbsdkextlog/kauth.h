/*
 * Created 190415 lynnl
 *
 * Implementation of KAuth callbacks
 */

#ifndef KAUTH_H
#define KAUTH_H

#include <sys/systm.h>

/*
 * __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ is a compiler-predefined macro
 */
#define OS_VER_MIN_REQ      __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#ifndef __MAC_10_14
#define __MAC_10_14         101400
#endif

kern_return_t kauth_register(void);
void kauth_deregister(void);

#endif /* KAUTH_H */

