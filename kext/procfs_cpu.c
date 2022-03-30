/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * procfs_cpu.c
 *
 * Functions that provide information about the CPU.
 */
#include <string.h>
#include <i386/cpuid.h>
#include <i386/tsc.h>
#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>
#include <libkext/libkext.h>
#include <mach/machine.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/proc_reg.h>
#include <sys/sbuf.h>

#include <miscfs/procfs/procfs.h>

#include "symdecls.h"

STATIC char *get_cpu_flags(void);
STATIC char *get_cpu_ext_flags(void);
STATIC char *get_leaf7_flags(void);
STATIC char *get_leaf7_ext_flags(void);

/* For AMD CPU's */
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

/* For Intel CPU's */
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

int
procfs_docpuinfo(pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    vm_offset_t pageno, uva;
    int len = 0, xlen = 0;
    off_t page_offset = 0;
    size_t buffer_size = 0;
    char *buffer;

    int pid = pnp->node_id.nodeid_pid;
    proc_t p = proc_find(pid);

    /*
     * Overall processor count for the current CPU.
     *
     * Not to be conflated with cpu_cores (number of cores)
     * as these are not the same.
     */
    uint32_t max_cpus = *_processor_count;

    /*
     * Initialize the processor counter.
     * This should always begin at 0 and
     * add 1 for each loop according to the
     * number of processors present.
     */
    uint32_t cnt_cpus = 0;

    /*
     * The core id should always start at 0.
     */
    int core_id = 0;

    /*
     * Initialize the TSC frequency variables.
     */
    uint64_t freq = *_tscFreq;
    int fqmhz = 0, fqkhz = 0;

    /* 
     * Set the TSC frequency variables
     */
    if (freq != 0) {
        fqmhz = (freq + 4999) / 1000000;
        fqkhz = ((freq + 4999) / 10000) % 100;
    }

    /*
     * The apicid variable begins at 0 and get increased
     * by 2 for each loop until the number becomes greater
     * than max_cpus, in which case the loop resets the
     * variable to a value of 1 and then contines increasing
     * that number by 2 for each loop.
     */
    int apicid = 0, initial_apicid = 0;

    /* 
     * Here we can utilize the i386_cpu_info structure in i386/cpuid.h
     * to get the information we need. The cpuid_info() function sets up
     * the i386_cpu_info structure and returns a pointer to the structure.
     */
    char *vendor_id = _cpuid_info()->cpuid_vendor;
    uint8_t cpu_family = _cpuid_info()->cpuid_family;
    uint8_t model = _cpuid_info()->cpuid_model; // FIXME
    char *model_name = _cpuid_info()->cpuid_brand_string;
    uint32_t microcode = _cpuid_info()->cpuid_microcode_version; // FIXME
    uint32_t cache_size = _cpuid_info()->cpuid_cache_size;
    uint8_t stepping = _cpuid_info()->cpuid_stepping;
    uint32_t cpu_cores = _cpuid_info()->core_count;
    uint32_t cpuid_level = cpu_cores;
    uint32_t tlb_size = _cpuid_info()->cache_linesize * 40;
    uint32_t clflush_size = _cpuid_info()->cache_linesize;
    uint32_t cache_alignment = clflush_size;
    uint32_t addr_bits_phys = _cpuid_info()->cpuid_address_bits_physical;
    uint32_t addr_bits_virt = _cpuid_info()->cpuid_address_bits_virtual;

    /*
     * Check if the FPU feature is present.
     */
    char *fpu, *fpu_exception;
    if (_cpuid_info()->cpuid_features & CPUID_FEATURE_FPU) {
        fpu = "yes";
        fpu_exception = "yes";
    } else {
        fpu = "no";
        fpu_exception = "no";
    }

    /*
     * Check for CPU write protection.
     */
    char *wp;
    if (get_cr0() & CR0_WP) {
        wp = "yes";
    } else {
        wp = "no";
    }

    /*
     * CPU flags.
     *
     * Bug note: not all of the flag variables stick
     * after the first iteration of the main loop if we,
     * define them here, likely due to a memory-related
     * issue that has been pointed out to me, though I'm
     * still unsure on how to go about fixing it.
     * For now, we'll declare the pointer variables here
     * and fetch the flags inside the loop.
     */
    char *cpuflags, *cpuextflags, *leaf7flags, *leaf7extflags;

    /* TODO */
    //char *pm = get_power_flags();
    char *pm = "";
    //char *x86_bugs = get_x86_bug_flags();
    char *x86_bugs = "";
    //char *x86_64_bugs = get_x86_64_bug_flags();
    char *x86_64_bugs = "";

    /*
     * Allocate memory.
     */
    buffer_size = (LBFSZ * 4);
    buffer = _MALLOC(buffer_size, M_TEMP, M_WAITOK);

    /*
     * Get the userspace virtual address.
     */
    uva = uio_offset(uio);

    /*
     * Get the page number of this segment.
     */
    pageno = trunc_page(uva);
    page_offset = uva - pageno;

    /*
     * The main loop iterates over each processor number stored in the
     * max_cpus variable and prints out a list of data for each processor.
     *
     * For example:
     *
     * If your CPU only has one processor, it should print only a single list.
     * If your CPU has two processors, it will print out a set of two lists,
     * and so on and so forth. My CPU processor number is 32 so I get 32 lists
     * with processor numbers starting from 0 - 31.
     */
    while (cnt_cpus < max_cpus) {
        /*
         * Fetch the CPU flags.
         */
        cpuflags = get_cpu_flags();
        cpuextflags = get_cpu_ext_flags();
        leaf7flags = get_leaf7_flags();
        leaf7extflags = get_leaf7_ext_flags();

        if (cnt_cpus <= max_cpus) {
            /* 
             * The data which to copy over to userspace.
             */
            len += snprintf(buffer, buffer_size,
                "processor\t\t: %u\n"
                "vendor_id\t\t: %s\n"
                "cpu family\t\t: %u\n"
                "model\t\t\t: %u\n"
                "model name\t\t: %s\n"
                "microcode\t\t: %u\n"
                "stepping\t\t: %u\n"
                "cpu MHz\t\t\t: %d.%02d\n"
                "cache size\t\t: %d KB\n"
                "physical id\t\t: %u\n"
                "siblings\t\t: %u\n"
                "core id\t\t\t: %d\n"
                "cpu cores\t\t: %u\n"
                "apicid\t\t\t: %u\n"
                "initial apicid\t\t: %u\n"
                "fpu\t\t\t: %s\n"
                "fpu_exception\t\t: %s\n"
                "cpuid level\t\t: %u\n"
                "wp\t\t\t: %s\n"
                "flags\t\t\t: %s%s%s%s\n"
                "bugs\t\t\t: %s%s\n"
                "bogomips\t\t: %d.%02d\n"
                "TLB size\t\t: %u 4K pages\n"
                "clflush_size\t\t: %u\n"
                "cache_alignment\t\t: %d\n"
                "address sizes\t\t: %d bits physical, %d bits virtual\n"
                "power management\t: %s\n\n",
                cnt_cpus,               // processor
                vendor_id,              // vendor_id
                cpu_family,             // cpu family
                model,                  // model
                model_name,             // model name
                microcode,              // microcode
                stepping,               // stepping
                fqmhz, fqkhz,           // cpu MHz
                cache_size,             // cache size
                0,                      // physical id
                max_cpus,               // siblings
                core_id,                // core id
                cpu_cores,              // cpu cores
                apicid,                 // apicid
                initial_apicid,         // initial apicid
                fpu,                    // fpu
                fpu_exception,          // fpu exception
                cpuid_level,            // cpuid level
                wp,                     // wp
                cpuflags,               // flags
                cpuextflags,            // flags
                leaf7flags,             // flags
                leaf7extflags,          // flags
                x86_bugs,               // bugs
                x86_64_bugs,            // bugs
                fqmhz * 2, fqkhz,       // bogomips
                tlb_size,               // TLB size
                clflush_size,           // clflush_size
                cache_alignment,        // cache_alignment
                addr_bits_phys,         // address size physical
                addr_bits_virt,         // address size virtual
                pm                      // power management
            );

            /*
             * How many bytes to copy.
             */
            xlen = imin(len - page_offset, uio_resid(uio));

            /*
             * Do the move into userspace.
             */
            uiomove(buffer, xlen, uio);

            /* 
             * Set len back to 0 before entering into the next loop.
             */
            if (len != 0) {
                len = 0;
            }

            /*
             * Increase by 2 for each loop.
             */
            apicid += 2;
            if (apicid >= max_cpus) {
                /* If the number exceeds max_cpus, reset to 1. */
                apicid = 1;
            }

            /*
             * The initial apicid is the same as apicid.
             */
            initial_apicid = apicid;

            /*
             * Add 1 to the CPU counter for each iteration.
             */
            cnt_cpus++;

            /*
             * Add 1 to the core_id counter for each iteration.
             */
            core_id++;

            /*
             * Reset the core_id counter if it exceeds the maximum number of cores.
             */
            if (core_id > cpu_cores - 1) {
                core_id = 0;
            }

            /*
             * Reset the max_cpus variable at the end of each loop.
             * An unknown bug is causing the variable to reduce the
             * count down to 23 unless we do this.
             */
            if (max_cpus != *_processor_count) {
                max_cpus = *_processor_count;
            }
        } else if (cnt_cpus > max_cpus) {
            /*
             * If the counter exceeds the processor count,
             * free the associated memory and break the loop.
             *
             * Note: Freeing the memory after breaking the loop
             * results in a kernel panic for some reason.
             */
            _FREE(&buffer, M_TEMP);
            break;
        }
    }
    proc_rele(p);

    return 0;
}

const char *feature_flags[] = {
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
    /* 56 */ "avx1.0",
    /* 57 */ "f16c",
    /* 58 */ "rdrand",
    /* 59 */ "vmm",
};

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

STATIC char *
get_cpu_flags(void)
{
    int i = 0;
    int size = 0;

    if (_cpuid_info()->cpuid_features) {
        size = (sizeof(feature_flags) * 2);

        char *flags[size];

        while (i < nitems(feature_flags)) {
            /* 
             * If the CPU supports a feature in the feature_list[]...
             */
            if (_cpuid_info()->cpuid_features & feature_list[i]) {
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

const char *feature_ext_flags[] = {
    /* 0 */ "syscall",
    /* 1 */ "xd",
    /* 2 */ "1gbpage",
    /* 3 */ "rdtscp",
    /* 4 */ "em64t",
    /* 5 */ "lahf",
    /* 6 */ "lzcnt",
    /* 7 */ "prefetchcw"
};

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

STATIC char *
get_cpu_ext_flags(void)
{
    int i = 0;
    int size = 0;

    if (_cpuid_info()->cpuid_extfeatures) {
        size = (sizeof(feature_ext_flags) * 2);

        char *flags[size];

        while (i < nitems(feature_ext_flags)) {
            /* 
             * If the CPU supports a feature in the feature_ext_list[]...
             */
            if (_cpuid_info()->cpuid_extfeatures & feature_ext_list[i]) {
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

const char *leaf7_feature_flags[] = {
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

STATIC char*
get_leaf7_flags(void)
{
    int i = 0;
    int size = 0;

    if (_cpuid_info()->cpuid_leaf7_features) {
        size = (sizeof(leaf7_feature_flags) * 2);
        char *flags[size];

        while (i < nitems(leaf7_feature_flags)) {
            /* 
             * If the CPU supports a feature in the leaf7_feature_list[]...
             */
            if (_cpuid_info()->cpuid_leaf7_features & leaf7_feature_list[i]) {
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

const char *leaf7_feature_ext_flags[] = {
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

STATIC char *
get_leaf7_ext_flags(void)
{
    int i = 0;
    int size = 0;

    if (_cpuid_info()->cpuid_leaf7_extfeatures) {
        size = (sizeof(leaf7_feature_ext_flags) * 2);
        char *flags[size];

        while (i < nitems(leaf7_feature_ext_flags)) {
            /* 
             * If the CPU supports a feature in the leaf7_feature_ext_list[]...
             */
            if (_cpuid_info()->cpuid_leaf7_extfeatures & leaf7_feature_ext_list[i]) {
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

const char *amd_feature_flags[] = {
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

static char *amd_feature2_flags[] = {
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

const char *power_flags[] = {
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

const char *x86_bug_flags[] = {
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

const char *x86_64_bug_flags[] = {
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
