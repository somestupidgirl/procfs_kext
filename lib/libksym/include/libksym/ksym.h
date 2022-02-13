/*
 * Created 190112 lynnl
 */
#ifndef KSYMRESOLVER_H
#define KSYMRESOLVER_H

#include <mach-o/loader.h>
#include <libksym/utils.h>

#define MAX_SLIDE_STEP                  4096

/* Exported: xnu/osfmk/mach/i386/vm_param.h */
#ifndef VM_MIN_KERNEL_ADDRESS
#define VM_MIN_KERNEL_ADDRESS           ((vm_offset_t) 0xFFFFFF8000000000UL)
#endif
#ifndef VM_MIN_KERNEL_AND_KEXT_ADDRESS
#define VM_MIN_KERNEL_AND_KEXT_ADDRESS  (VM_MIN_KERNEL_ADDRESS - 0x80000000ULL)
#endif

#define KERN_HIB_BASE                   ((vm_offset_t) 0xFFFFFF8000100000ULL)
#define KERN_TEXT_BASE                  ((vm_offset_t) 0xFFFFFF8000200000ULL)

#define MAGIC                           (MH_MAGIC_64 || MH_CIGAM_64)
#define FILETYPE                         MH_EXECUTE
#define LE_ADDR                         ((vm_address_t) 0xFFFFFF8000FA5000ULL)

/**
 * Resolve a kernel symbol address
 * @param name          symbol name(should begin with _)
 * @return              NULL if not found
 */
vm_offset_t get_vm_kernel_addrperm_ext(void);
vm_offset_t get_vm_kernel_slide(void);
struct segment_command_64 *find_seg64(struct mach_header_64 *mh, const char *name);
struct load_command *find_lc(struct mach_header_64 *mh, uint32_t cmd);
void * __nullable resolve_ksymbol(const char * __nonnull);

#endif  /* KSYMRESOLVER_H */

