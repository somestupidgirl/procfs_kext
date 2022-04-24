#ifndef cpu_h
#define cpu_h

#include <stdint.h>

#define AMD_UCODE_MAGIC				0x00414d44
#define MSR_UCODE_AMD_PATCHLEVEL	0x0000008b
#define MSR_UCODE_AMD_PATCHLOADER	0xc0010020

#pragma mark -
#pragma mark Public API

extern struct loadavg averunnable;
extern fixpt_t cexp[3];

extern boolean_t is_amd_cpu(void);
extern boolean_t is_intel_cpu(void);

extern uint32_t extract_bitfield(uint32_t infield, uint32_t width, uint32_t offset);
extern uint32_t get_bitfield_width(uint32_t number);

extern uint32_t set_microcode_version(void);
extern uint32_t get_microcode_version(void);

extern char *get_cpu_flags(void);
extern char *get_cpu_ext_flags(void);
extern char *get_leaf7_flags(void);
extern char *get_leaf7_ext_flags(void);

#endif /* cpu_h */