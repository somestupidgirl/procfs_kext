/*
 * Copyright (c) 2014 squiffypwn. All rights reserved.
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * cpuinfo.c
 *
 * CPU kext library containing various odds and ends
 * for various CPU-related things and with AMD support
 * in mind.
 *
 * Certain functions have been pulled from the AMD
 * patches for XNU authored by Shaneee, AlGrey and
 * others. Specifically the following functions:
 *
 *      is_amd_cpu()
 *      is_intel_cpu()
 *      extract_bitfield()
 *      get_bitfield_width()
 */
#include <stddef.h>
#include <string.h>
#include <i386/cpuid.h>
#include <i386/pmCPU.h>
#include <i386/tsc.h>
#include <libkext/libkext.h>
#include <mach/machine.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/proc_reg.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/types.h>

#include "cpu.h"
#include "symbols.h"

/*
 * Multipliers used to encode 1*K .. 64*M in a 16 bit size field
 */
#define K               (1)
#define M               (1024)

#define min(a, b) ((a) < (b) ? (a) : (b))
#define quad(hi, lo)    (((uint64_t)(hi)) << 32 | (lo))

/*
 * Load average of runnable processes.
 */
struct loadavg averunnable = {
    { 0, 0, 0}, FSCALE };

/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
fixpt_t
cexp[3] = {
    (fixpt_t)(0.9200444146293232 * FSCALE), /* exp(-1/12) */
    (fixpt_t)(0.9834714538216174 * FSCALE), /* exp(-1/60) */
    (fixpt_t)(0.9944598480048967 * FSCALE), /* exp(-1/180) */
};

/*
 * Returns TRUE if AMD CPU.
 */
boolean_t
is_amd_cpu(void)
{
    uint32_t ourcpuid[4];

    do_cpuid(0, ourcpuid);
    if (ourcpuid[ebx] == 0x68747541 &&
        ourcpuid[ecx] == 0x444D4163 &&
        ourcpuid[edx] == 0x69746E65) {
        return TRUE;
    }

    return FALSE;
};

/*
 * Returns TRUE if Intel CPU.
 */
boolean_t
is_intel_cpu(void)
{
    uint32_t ourcpuid[4];

    do_cpuid(0, ourcpuid);
    if (ourcpuid[ebx] == 0x756E6547 &&
        ourcpuid[ecx] == 0x6C65746E &&
        ourcpuid[edx] == 0x49656E69) {
        return TRUE;
    }

    if (!is_amd_cpu()) {
        return TRUE;
    }

    return FALSE;
}

/*
 * Extract bitfield function.
 */
uint32_t
extract_bitfield(uint32_t infield, uint32_t width, uint32_t offset)
{
    uint32_t bitmask;
    uint32_t outfield;

    if ((offset+width) == 32) {
        bitmask = (0xFFFFFFFF<<offset);
    } else {
        bitmask = (0xFFFFFFFF<<offset) ^ (0xFFFFFFFF<<(offset+width));
    }

    outfield = (infield & bitmask) >> offset;
    return outfield;
}

/*
 * Get bitfield width function.
 */
uint32_t
get_bitfield_width(uint32_t number)
{
    uint32_t fieldwidth;

    number--;
    if (number == 0) {
        return 0;
    }

    __asm__ volatile ( "bsr %%eax, %%ecx\n\t"
                      : "=c" (fieldwidth)
                      : "a"(number));

    return fieldwidth+1;  /* bsr returns the position, we want the width */
}

/*
 * Disable CPU write protection.
 * From squiffypwn's libmasochist.
 */
uint32_t
disable_write_protection() {
    uintptr_t cr0 = get_cr0();
    cr0 &= ~CR0_WP;
    set_cr0(cr0);

    return 0;
}

/*
 * Enable CPU write protection.
 * From squiffypwn's libmasochist.
 */
uint32_t
enable_write_protection() {
    uintptr_t cr0 = get_cr0();
    cr0 |= CR0_WP;
    set_cr0(cr0);

    return 0;
}

/*
 * This function returns the actual microcode version
 * number for both AMD and Intel CPUs without modify-
 * ing the cpuid_microcode_version field in struct
 * i386_cpu_info. 
 */
uint32_t
get_microcode_version(void)
{
    uint32_t version = 0;

    if (is_amd_cpu()) {
        version = (uint32_t)rdmsr64(MSR_UCODE_AMD_PATCHLEVEL);
    } else if (is_intel_cpu()) {
        version = (uint32_t)(rdmsr64(MSR_IA32_BIOS_SIGN_ID) >> 32);
    }

    return version;
}

/*
 * WARNING! This is currently for R&D purposes only.
 *
 * In the AMD kernel the cpuid_microcode_version
 * field in struct i386_cpu_info is set to 186.
 * This is obviously a dummy version number, but
 * setting it to its actual version number will
 * result in a kernel panic for some unknown rea-
 * son. Needs further research.
 */
uint32_t
set_microcode_version(void)
{
    i386_cpu_info_t *info_p;
    uint32_t reg[4];

    /*
     * Get processor signature and decode
     * and bracket this with the approved procedure for reading the
     * the microcode version number a.k.a. signature a.k.a. BIOS ID
     */
    if (is_intel_cpu()) {
        wrmsr64(MSR_IA32_BIOS_SIGN_ID, 0);
        do_cpuid(1, reg);
        info_p->cpuid_microcode_version = (uint32_t)(rdmsr64(MSR_IA32_BIOS_SIGN_ID) >> 32);
    } else if (is_amd_cpu()) {
        wrmsr64(MSR_UCODE_AMD_PATCHLOADER, 0);
        do_cpuid(1, reg);
        info_p->cpuid_microcode_version = (uint32_t)(rdmsr64(MSR_UCODE_AMD_PATCHLEVEL));
    }

    return 0;
}

/*
 * CPU features and flags.
 */

uint64_t feature_list[] = {
    /* 0 */  CPUID_FEATURE_FPU,
    /* 1 */  CPUID_FEATURE_VME,
    /* 2 */  CPUID_FEATURE_DE,
    /* 3 */  CPUID_FEATURE_PSE,
    /* 4 */  CPUID_FEATURE_TSC,
    /* 5 */  CPUID_FEATURE_MSR,
    /* 6 */  CPUID_FEATURE_PAE,
    /* 7 */  CPUID_FEATURE_MCE,
    /* 8 */  CPUID_FEATURE_CX8,
    /* 9 */  CPUID_FEATURE_APIC,
    /* 10 */ CPUID_FEATURE_SEP,
    /* 11 */ CPUID_FEATURE_MTRR,
    /* 12 */ CPUID_FEATURE_PGE,
    /* 13 */ CPUID_FEATURE_MCA,
    /* 14 */ CPUID_FEATURE_CMOV,
    /* 15 */ CPUID_FEATURE_PAT,
    /* 16 */ CPUID_FEATURE_PSE36,
    /* 17 */ CPUID_FEATURE_PSN,
    /* 18 */ CPUID_FEATURE_CLFSH,
    /* 19 */ CPUID_FEATURE_DS,
    /* 20 */ CPUID_FEATURE_ACPI,
    /* 21 */ CPUID_FEATURE_MMX,
    /* 22 */ CPUID_FEATURE_FXSR,
    /* 23 */ CPUID_FEATURE_SSE,
    /* 24 */ CPUID_FEATURE_SSE2,
    /* 25 */ CPUID_FEATURE_SS,
    /* 26 */ CPUID_FEATURE_HTT,
    /* 27 */ CPUID_FEATURE_TM,
    /* 28 */ CPUID_FEATURE_PBE,
    /* 29 */ CPUID_FEATURE_SSE3,
    /* 30 */ CPUID_FEATURE_PCLMULQDQ,
    /* 31 */ CPUID_FEATURE_DTES64,
    /* 32 */ CPUID_FEATURE_MONITOR,
    /* 33 */ CPUID_FEATURE_DSCPL,
    /* 34 */ CPUID_FEATURE_VMX,
    /* 35 */ CPUID_FEATURE_SMX,
    /* 36 */ CPUID_FEATURE_EST,
    /* 37 */ CPUID_FEATURE_TM2,
    /* 38 */ CPUID_FEATURE_SSSE3,
    /* 39 */ CPUID_FEATURE_CID,
    /* 40 */ CPUID_FEATURE_SEGLIM64,
    /* 41 */ CPUID_FEATURE_FMA,
    /* 42 */ CPUID_FEATURE_CX16,
    /* 43 */ CPUID_FEATURE_xTPR,
    /* 44 */ CPUID_FEATURE_PDCM,
    /* 45 */ CPUID_FEATURE_PCID,
    /* 46 */ CPUID_FEATURE_DCA,
    /* 47 */ CPUID_FEATURE_SSE4_1,
    /* 48 */ CPUID_FEATURE_SSE4_2,
    /* 49 */ CPUID_FEATURE_x2APIC,
    /* 50 */ CPUID_FEATURE_MOVBE,
    /* 51 */ CPUID_FEATURE_POPCNT,
    /* 52 */ CPUID_FEATURE_TSCTMR,
    /* 53 */ CPUID_FEATURE_AES,
    /* 54 */ CPUID_FEATURE_XSAVE,
    /* 55 */ CPUID_FEATURE_OSXSAVE,
    /* 56 */ CPUID_FEATURE_AVX1_0,
    /* 57 */ CPUID_FEATURE_F16C,
    /* 58 */ CPUID_FEATURE_RDRAND,
    /* 59 */ CPUID_FEATURE_VMM,
};

const char *
feature_flags[] = {
    /* 0 */  "fpu",
    /* 1 */  "vme",
    /* 2 */  "de",
    /* 3 */  "pse",
    /* 4 */  "tsc",
    /* 5 */  "msr",
    /* 6 */  "pae",
    /* 7 */  "mce",
    /* 8 */  "cx8",
    /* 9 */  "apic",
    /* 10 */ "sep",
    /* 11 */ "mtrr",
    /* 12 */ "pge",
    /* 13 */ "mca",
    /* 14 */ "cmov",
    /* 15 */ "pat",
    /* 16 */ "pse36",
    /* 17 */ "psn",
    /* 18 */ "clfsh",
    /* 19 */ "ds",
    /* 20 */ "acpi",
    /* 21 */ "mmx",
    /* 22 */ "fxsr",
    /* 23 */ "sse",
    /* 24 */ "sse2",
    /* 25 */ "ss",
    /* 26 */ "htt",
    /* 27 */ "tm",
    /* 28 */ "pbe",
    /* 39 */ "sse3",
    /* 30 */ "pclmulqdq",
    /* 31 */ "dtes64",
    /* 32 */ "mon",
    /* 33 */ "dscpl",
    /* 34 */ "vmx",
    /* 35 */ "smx",
    /* 36 */ "est",
    /* 37 */ "tm2",
    /* 38 */ "ssse3",
    /* 39 */ "cid",
    /* 40 */ "seglim64",
    /* 41 */ "fma",
    /* 42 */ "cx16",
    /* 43 */ "tpr",
    /* 44 */ "pdcm",
    /* 45 */ "pcid",
    /* 46 */ "dca",
    /* 47 */ "sse4.1",
    /* 48 */ "sse4.2",
    /* 49 */ "x2apic",
    /* 50 */ "movbe",
    /* 51 */ "popcnt",
    /* 52 */ "tsctmr",
    /* 53 */ "aes",
    /* 54 */ "xsave",
    /* 55 */ "osxsave",
    /* 56 */ "avx",
    /* 57 */ "f16c",
    /* 58 */ "rdrand",
    /* 59 */ "vmm",
};

char *
get_cpu_flags(void)
{
    int i = 0;
    int size = 0;

    /*
     * Main loop.
     */
    if (cpuid_info()->cpuid_features) {
        size = (sizeof(feature_flags) * 2);

        char *flags[size];

        while (i < ARRAY_COUNT(feature_flags)) {
            /* 
             * If the CPU supports a feature in the feature_list[]...
             */
            if (cpuid_info()->cpuid_features & feature_list[i]) {
                /*
                 * ...amend its flag to 'flags'.
                 */
                strlcat(flags, feature_flags[i], sizeof(flags));
                strlcat(flags, " ", sizeof(flags));
            }
            /*
             * Add 1 to the counter for each iteration.
             */
            i++;
        }
        return flags;

    } else {
        size = 8;
        char *flags[size];
        strlcat(flags,"", sizeof(flags));
        return flags;
    }
}

/*
 * An array of extended features as defined in
 * osfmk/i386/cpuid.h. This should always be
 * in the same order as its corresponding
 * character array below.
 */
uint64_t feature_ext_list[] = {
    /* 0 */ CPUID_EXTFEATURE_SYSCALL,
    /* 1 */ CPUID_EXTFEATURE_XD,
    /* 2 */ CPUID_EXTFEATURE_1GBPAGE,
    /* 3 */ CPUID_EXTFEATURE_RDTSCP,
    /* 4 */ CPUID_EXTFEATURE_EM64T,
    /* 5 */ CPUID_EXTFEATURE_LAHF,
    /* 6 */ CPUID_EXTFEATURE_LZCNT,
    /* 7 */ CPUID_EXTFEATURE_PREFETCHW
};

const char *
feature_ext_flags[] = {
    /* 0 */ "syscall",
    /* 1 */ "xd",
    /* 2 */ "1gbpage",
    /* 3 */ "rdtscp",
    /* 4 */ "em64t",
    /* 5 */ "lahf",
    /* 6 */ "lzcnt",
    /* 7 */ "prefetchcw"
};

char *
get_cpu_ext_flags(void)
{
    int i = 0;
    int size = 0;

    if (cpuid_info()->cpuid_extfeatures) {
        size = (sizeof(feature_ext_flags) * 2);

        char *flags[size];

        /*
         * Main loop.
         */
        while (i < ARRAY_COUNT(feature_ext_flags)) {
            /* 
             * If the CPU supports a feature in the feature_ext_list[]...
             */
            if (cpuid_info()->cpuid_extfeatures & feature_ext_list[i]) {
                /*
                 * ...amend its flag to 'flags'.
                 */
                strlcat(flags, feature_ext_flags[i], sizeof(flags));
                strlcat(flags, " ", sizeof(flags));
            }
            /*
             * Add 1 to the counter for each iteration.
             */
            i++;
        }
        return flags;

    } else {
        size = 8;
        char *flags[size];

        strlcat(flags,"", sizeof(flags));

        return flags;
    }
}

uint64_t leaf7_feature_list[] = {
    /* 0 */  CPUID_LEAF7_FEATURE_RDWRFSGS,
    /* 1 */  CPUID_LEAF7_FEATURE_TSCOFF,
    /* 2 */  CPUID_LEAF7_FEATURE_SGX,
    /* 3 */  CPUID_LEAF7_FEATURE_BMI1,
    /* 4 */  CPUID_LEAF7_FEATURE_HLE,
    /* 5 */  CPUID_LEAF7_FEATURE_AVX2,
    /* 6 */  CPUID_LEAF7_FEATURE_FDPEO,
    /* 7 */  CPUID_LEAF7_FEATURE_SMEP,
    /* 8 */  CPUID_LEAF7_FEATURE_BMI2,
    /* 9 */  CPUID_LEAF7_FEATURE_ERMS,
    /* 10 */ CPUID_LEAF7_FEATURE_INVPCID,
    /* 11 */ CPUID_LEAF7_FEATURE_RTM,
    /* 12 */ CPUID_LEAF7_FEATURE_PQM,
    /* 13 */ CPUID_LEAF7_FEATURE_FPU_CSDS,
    /* 14 */ CPUID_LEAF7_FEATURE_MPX,
    /* 15 */ CPUID_LEAF7_FEATURE_PQE,
    /* 16 */ CPUID_LEAF7_FEATURE_AVX512F,
    /* 17 */ CPUID_LEAF7_FEATURE_AVX512DQ,
    /* 18 */ CPUID_LEAF7_FEATURE_RDSEED,
    /* 19 */ CPUID_LEAF7_FEATURE_ADX,
    /* 20 */ CPUID_LEAF7_FEATURE_SMAP,
    /* 21 */ CPUID_LEAF7_FEATURE_AVX512IFMA,
    /* 22 */ CPUID_LEAF7_FEATURE_CLFSOPT,
    /* 23 */ CPUID_LEAF7_FEATURE_CLWB,
    /* 24 */ CPUID_LEAF7_FEATURE_IPT,
    /* 25 */ CPUID_LEAF7_FEATURE_AVX512CD,
    /* 26 */ CPUID_LEAF7_FEATURE_SHA,
    /* 27 */ CPUID_LEAF7_FEATURE_AVX512BW,
    /* 28 */ CPUID_LEAF7_FEATURE_AVX512VL,
    /* 29 */ CPUID_LEAF7_FEATURE_PREFETCHWT1,
    /* 30 */ CPUID_LEAF7_FEATURE_AVX512VBMI,
    /* 31 */ CPUID_LEAF7_FEATURE_UMIP,
    /* 32 */ CPUID_LEAF7_FEATURE_PKU,
    /* 33 */ CPUID_LEAF7_FEATURE_OSPKE,
    /* 34 */ CPUID_LEAF7_FEATURE_WAITPKG,
    /* 35 */ CPUID_LEAF7_FEATURE_GFNI,
    /* 36 */ CPUID_LEAF7_FEATURE_VAES,
    /* 37 */ CPUID_LEAF7_FEATURE_VPCLMULQDQ,
    /* 38 */ CPUID_LEAF7_FEATURE_AVX512VNNI,
    /* 39 */ CPUID_LEAF7_FEATURE_AVX512BITALG,
    /* 40 */ CPUID_LEAF7_FEATURE_AVX512VPCDQ,
    /* 41 */ CPUID_LEAF7_FEATURE_RDPID,
    /* 42 */ CPUID_LEAF7_FEATURE_CLDEMOTE,
    /* 43 */ CPUID_LEAF7_FEATURE_MOVDIRI,
    /* 44 */ CPUID_LEAF7_FEATURE_MOVDIRI64B,
    /* 45 */ CPUID_LEAF7_FEATURE_SGXLC
};

const char *
leaf7_feature_flags[] = {
    /* 0 */  "rdwrfsgs",
    /* 1 */  "tsc_thread_offset",
    /* 2 */  "sgx",
    /* 3 */  "bmi1",
    /* 4 */  "hle",
    /* 5 */  "avx2",
    /* 6 */  "fdpeo",
    /* 7 */  "smep",
    /* 8 */  "bmi2",
    /* 9 */  "erms",
    /* 10 */ "invpcid",
    /* 11 */ "rtm",
    /* 12 */ "pqm",
    /* 13 */ "fpu_csds",
    /* 14 */ "mpx",
    /* 15 */ "pqe",
    /* 16 */ "avx512f",
    /* 17 */ "avx512dq",
    /* 18 */ "rdseed",
    /* 19 */ "adx",
    /* 20 */ "smap",
    /* 21 */ "avx512ifma",
    /* 22 */ "clfsopt",
    /* 23 */ "clwb",
    /* 24 */ "ipt",
    /* 25 */ "avx512cd",
    /* 26 */ "sha",
    /* 27 */ "avx512bw",
    /* 28 */ "avx512vl",
    /* 29 */ "prefetchwt1",
    /* 30 */ "avx512vbmi",
    /* 31 */ "umip",
    /* 32 */ "pku",
    /* 33 */ "ospke",
    /* 34 */ "waitpkg",
    /* 35 */ "gfni",
    /* 36 */ "vaes",
    /* 37 */ "vpclmulqdq",
    /* 38 */ "avx512vnni",
    /* 39 */ "avx512bitalg",
    /* 40 */ "avx512vpopcntdq",
    /* 41 */ "rdpid",
    /* 42 */ "cldemote",
    /* 43 */ "movdiri",
    /* 44 */ "movdiri64b",
    /* 45 */ "sgxlc"
};

char*
get_leaf7_flags(void)
{
    int i = 0;
    int size = 0;

    /*
     * Enable reading the cpuid_leaf7_features on AMD chipsets.
     */
    uint32_t reg[4];
    if (is_amd_cpu() && cpuid_info()->cpuid_family >= 23){
        do_cpuid(0x7, reg);
        cpuid_info()->cpuid_leaf7_features = quad(reg[ecx], reg[ebx]) & ~CPUID_LEAF7_FEATURE_SMAP;
    }

    /*
     * Main loop.
     */
    if (cpuid_info()->cpuid_leaf7_features) {
        size = (sizeof(leaf7_feature_flags) * 2);
        char *flags[size];

        while (i < ARRAY_COUNT(leaf7_feature_flags)) {
            /* 
             * If the CPU supports a feature in the leaf7_feature_list[]...
             */
            if (cpuid_info()->cpuid_leaf7_features & leaf7_feature_list[i]) {
                /*
                 * ...amend its flag to 'flags'.
                 */
                strlcat(flags, leaf7_feature_flags[i], sizeof(flags));
                strlcat(flags, " ", sizeof(flags));
            }
            /*
             * Add 1 to the counter for each iteration.
             */
            i++;
        }
        return flags;

    } else {
        size = 8;
        char *flags[size];

        strlcat(flags,"", sizeof(flags));

        return flags;
    }
}

uint64_t leaf7_feature_ext_list[] = {
    /* 0 */  CPUID_LEAF7_EXTFEATURE_AVX5124VNNIW,
    /* 1 */  CPUID_LEAF7_EXTFEATURE_AVX5124FMAPS,
    /* 2 */  CPUID_LEAF7_EXTFEATURE_FSREPMOV,
    /* 3 */  CPUID_LEAF7_EXTFEATURE_SRBDS_CTRL,
    /* 4 */  CPUID_LEAF7_EXTFEATURE_MDCLEAR,
    /* 5 */  CPUID_LEAF7_EXTFEATURE_TSXFA,
    /* 6 */  CPUID_LEAF7_EXTFEATURE_IBRS,
    /* 7 */  CPUID_LEAF7_EXTFEATURE_STIBP,
    /* 8 */  CPUID_LEAF7_EXTFEATURE_L1DF,
    /* 9 */  CPUID_LEAF7_EXTFEATURE_ACAPMSR,
    /* 10 */ CPUID_LEAF7_EXTFEATURE_CCAPMSR,
    /* 11 */ CPUID_LEAF7_EXTFEATURE_SSBD
};

const char *
leaf7_feature_ext_flags[] = {
    /* 0 */  "avx5124vnniw",
    /* 1 */  "avx5124fmaps",
    /* 2 */  "fsrepmov",
    /* 3 */  "srbds_ctlr",
    /* 4 */  "mdclear",
    /* 5 */  "tsxfa",
    /* 6 */  "ibrs",
    /* 7 */  "stibp",
    /* 8 */  "l1d",
    /* 9 */  "acapmsr",
    /* 10 */ "ccapmsr",
    /* 11 */ "ssbd"
};

char *
get_leaf7_ext_flags(void)
{
    int i = 0;
    int size = 0;

    /*
     * FIXME: Enable reading the cpuid_leaf7_extfeatures on AMD chipsets.
     */
#if 0
    uint32_t reg[4];
    if (is_amd_cpu() && cpuid_info()->cpuid_family >= 23){
        do_cpuid(0x7, reg);
        cpuid_info()->cpuid_leaf7_extfeatures = reg[ebx];
    }
#endif

    /*
     * Main loop.
     */
    if (cpuid_info()->cpuid_leaf7_extfeatures) {
        size = (sizeof(leaf7_feature_ext_flags) * 2);
        char *flags[size];

        while (i < ARRAY_COUNT(leaf7_feature_ext_flags)) {
            /* 
             * If the CPU supports a feature in the leaf7_feature_ext_list[]...
             */
            if (cpuid_info()->cpuid_leaf7_extfeatures & leaf7_feature_ext_list[i]) {
                /*
                 * ...amend its flag to 'flags'.
                 */
                strlcat(flags[size], leaf7_feature_ext_flags[i], sizeof(flags));
                strlcat(flags, " ", sizeof(flags));
            }
            /*
             * Add 1 to the counter for each iteration.
             */
            i++;
        }
        return flags;

    } else {
        size = 8;
        char *flags[size];

        strlcat(flags,"", sizeof(flags));

        return flags;
    }
}

/*
 * TODO
 */

const char *
amd_feature_flags[] = {
    /* 0 */ "syscall",
    /* 1 */ "mp",
    /* 2 */ "nx",
    /* 3 */ "mmxext",
    /* 4 */ "fxsr_opt",
    /* 5 */ "pdpe1gb",
    /* 6 */ "rdtscp",
    /* 7 */ "lm",
    /* 8 */ "3dnowext",
    /* 9 */ "3dnow",
};

static char *
amd_feature2_flags[] = {
    /* 0 */  "lahf_lm",
    /* 1 */  "cmp_legacy",
    /* 2 */  "svm",
    /* 3 */  "extapic",
    /* 4 */  "cr8_legacy",
    /* 5 */  "abm",
    /* 6 */  "sse4a",
    /* 7 */  "misalignsse",
    /* 8 */  "3dnowprefetch",
    /* 9 */  "osvw",
    /* 10 */ "ibs",
    /* 11 */ "xop",
    /* 12 */ "skinit",
    /* 13 */ "wdt",
    /* 14 */ "lwp",
    /* 15 */ "fma4",
    /* 16 */ "tce",
    /* 17 */ "nodeid_msr",
    /* 18 */ "tbm",
    /* 19 */ "topoext",
    /* 20 */ "perfctr_core",
    /* 21 */ "perfctr_nb",
    /* 22 */ "bpext",
    /* 23 */ "ptsc",
    /* 24 */ "perfctr_llc",
    /* 25 */ "mwaitx",
};

const char *
amd_power_flags[] = {
    /* 0 */  "ts",                  /* temperature sensor */
    /* 1 */  "fid",                 /* frequency id control */
    /* 2 */  "vid",                 /* voltage id control */
    /* 3 */  "ttp",                 /* thermal trip */
    /* 4 */  "tm",                  /* hardware thermal control */
    /* 5 */  "stc",                 /* software thermal control */
    /* 6 */  "100mhzsteps",         /* 100 MHz multiplier control */
    /* 7 */  "hwpstate",            /* hardware P-state control */
    /* 8 */  "",                    /* tsc invariant mapped to constant_tsc */
    /* 9 */  "cpb",                 /* core performance boost */
    /* 10 */ "eff_freq_ro",         /* readonly aperf/mperf */
    /* 11 */ "proc_feedback",       /* processor feedback interface */
    /* 12 */ "acc_power"            /* accumulated power mechanism */
};

const char *
x86_bug_flags[] = {
    /* 0 */ "f00f",                 /* Intel F00F */
    /* 1 */ "fdiv",                 /* FPU FDIV */
    /* 2 */ "coma",                 /* Cyrix 6x86 coma */
    /* 3 */ "tlb_mmatch",           /* "tlb_mmatch" AMD Erratum 383 */
    /* 4 */ "apic_c1e",             /* "apic_c1e" AMD Erratum 400 */
    /* 5 */ "11ap",                 /* Bad local APIC aka 11AP */
    /* 6 */ "fxsave_leak",          /* FXSAVE leaks FOP/FIP/FOP */
    /* 7 */ "clflush_monitor",      /* AAI65, CLFLUSH required before MONITOR */
    /* 8 */ "sysret_ss_attrs",      /* SYSRET doesn't fix up SS attrs */
    /* 9 */ "espfix"                /* "" IRET to 16-bit SS corrupts ESP/RSP high bits */
};

const char *
x86_64_bug_flags[] = {
    /* 0 */  "null_seg",            /* Nulling a selector preserves the base */
    /* 1 */  "swapgs_fence",        /* SWAPGS without input dep on GS */
    /* 2 */  "monitor",             /* IPI required to wake up remote CPU */
    /* 3 */  "amd_e400",            /* CPU is among the affected by Erratum 400 */
    /* 4 */  "cpu_meltdown",        /* CPU is affected by meltdown attack and needs kernel page table isolation */
    /* 5 */  "spectre_v1",          /* CPU is affected by Spectre variant 1 attack with conditional branches */
    /* 6 */  "spectre_v2",          /* CPU is affected by Spectre variant 2 attack with indirect branches */
    /* 7 */  "spec_store_bypass",   /* CPU is affected by speculative store bypass attack */
    /* 8 */  "l1tf",                /* CPU is affected by L1 Terminal Fault */
    /* 9 */  "mds",                 /* CPU is affected by Microarchitectural data sampling */
    /* 10 */ "msdbs_only",          /* CPU is only affected by the  MSDBS variant of BUG_MDS */
    /* 11 */ "swapgs",              /* CPU is affected by speculation through SWAPGS */
    /* 12 */ "taa",                 /* CPU is affected by TSX Async Abort(TAA) */
    /* 13 */ "itlb_multihit",       /* CPU may incur MCE during certain page attribute changes */
    /* 14 */ "srbds"                /* CPU may leak RNG bits if not mitigated */
};
