/*
 * Based on:
 *      https://github.com/Ch4nc3n/HookSysCall
 *      https://github.com/leiless/ksymresolver
 */

#ifndef KSYM_H
#define KSYM_H

#include <mach-o/loader.h>

typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;

/* Exported: xnu/osfmk/mach/i386/vm_param.h */
#ifndef VM_MIN_KERNEL_ADDRESS
#define VM_MIN_KERNEL_ADDRESS               ((vm_offset_t) 0xffffff8000000000UL)
#endif
#ifndef VM_MIN_KERNEL_AND_KEXT_ADDRESS
#define VM_MIN_KERNEL_AND_KEXT_ADDRESS      (VM_MIN_KERNEL_ADDRESS - 0x80000000ULL)
#endif

#define KERN_HIB_BASE                       ((vm_offset_t) 0xffffff8000100000ULL)
#define KERN_TEXT_BASE                      ((vm_offset_t) 0xffffff8000200000ULL)

#define MAX_SLIDE_STEP                      4096

/**
 * \brief   Resolve private kernel symbol for loaded kernel image
 */
void* resolve_kernel_symbol(const char* name);

mach_header_t *find_kernel_header(void);

#endif  /* KSYMRESOLVER_H */