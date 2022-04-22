#ifndef cpuinfo_h
#define cpuinfo_h

#include <stdint.h>

#define MSR_UCODE_AMD_PATCHLEVEL	0x0000008b
#define MSR_UCODE_AMD_PATCHLOADER	0xc0010020

#pragma mark -
#pragma mark Public API

extern boolean_t is_amd_cpu(void);
extern boolean_t is_intel_cpu(void);

extern uint32_t extract_bitfield(uint32_t infield, uint32_t width, uint32_t offset);
extern uint32_t get_bitfield_width(uint32_t number);

extern int get_microcode_version(void);

extern char *get_cpu_flags(void);
extern char *get_cpu_ext_flags(void);
extern char *get_leaf7_flags(void);
extern char *get_leaf7_ext_flags(void);

#endif /* cpuinfo_h */