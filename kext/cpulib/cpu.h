#ifndef cpuinfo_h
#define cpuinfo_h

#include <stdint.h>

#pragma mark -
#pragma mark Public API

extern boolean_t is_amd_cpu(void);
extern boolean_t is_intel_cpu(void);

extern char *get_cpu_flags(void);
extern char *get_cpu_ext_flags(void);
extern char *get_leaf7_flags(void);
extern char *get_leaf7_ext_flags(void);

#pragma mark -
#pragma mark Macros

#define quad(hi,lo)    (((uint64_t)(hi)) << 32 | (lo))

#endif /* cpuinfo_h */