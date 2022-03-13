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

int
procfs_docpuinfo(__unused procfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    vm_offset_t pageno, uva, kva;
    int len = 0, xlen = 0;
    off_t page_offset = 0;
    size_t buffer_size = 0;
    char *buffer;

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
     * Get the CPU flags.
     */
    char *cpuflags, *cpuextflags, *leaf7flags, *leaf7extflags;

    cpuflags = get_cpu_flags();
    cpuextflags = get_cpu_ext_flags();
    leaf7flags = get_leaf7_flags();
    leaf7extflags = get_leaf7_ext_flags();

    /*
     * Check for CPU write protection.
     */
    char *wp;
    if (get_cr0() & CR0_WP) {
        wp = "yes";
    } else {
        wp = "no";
    }

    /* TODO */
    //char *bugs = get_cpu_bugs();
    char *bugs = "";
    //char *pm = get_cpu_pm();
    char *pm = "";


    /*
     * Set up the variables required to move our data into userspace.
     */
    kva = VM_MIN_KERNEL_ADDRESS;                                        // kernel virtual address
    uva = uio_offset(uio);                                              // user virtual address
    pageno = trunc_page(uva);                                           // page number
    page_offset = uva - pageno;                                         // page offset
    buffer_size = (LBFSZ * 4);                                          // buffer size
    buffer = _MALLOC(buffer_size, M_TEMP, M_WAITOK);                    // buffer

    do {
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
                "flags\t\t\t: %s %s %s %s\n"
                "bugs\t\t\t: %s\n"
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
                bugs,                   // bugs
                fqmhz * 2, fqkhz,       // bogomips
                tlb_size,               // TLB size
                clflush_size,           // clflush_size
                cache_alignment,        // cache_alignment
                addr_bits_phys,         // address size physical
                addr_bits_virt,         // address size virtual
                pm                      // power management
            );

            /*
             * Subtract the uva offset from len.
             */
            xlen = len - uva;
            xlen = imin(xlen, uio_resid(uio));

            /*
             * Copy our data into userspace.
             */
            uiomove(buffer, xlen, uio);

            /* 
             * Set len back to 0 before entering into the next loop.
             */
            if (len != 0) {
                len = 0;
            }

            /*
             * Reset the max_cpus variable at the end of each loop.
             * Otherwise it tends to behave erratically.
             */
            if (max_cpus != *_processor_count) {
                max_cpus = *_processor_count;
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
             * Update the CPU counter.
             */
            cnt_cpus++;

            /*
             * Update the core_id.
             */
            core_id++;

            /*
             * The core_id should never exceed the number of cores.
             * Start over if it does.
             */
            if (core_id > cpu_cores - 1) {
                core_id = 0;
            }

            /*
             * Continue unless the counter exceeds the
             * available processor count.
             */
            continue;
        } else if (cnt_cpus > max_cpus) {
            /*
             * If the counter exceeds the processor count,
             * free the associated memory and break the loop.
             */
            _FREE(&buffer, M_TEMP);
            break;
        }
    } while (cnt_cpus < max_cpus);

    return 0;
}

const char *feature_flags[] = {
    /* 1 */  "fpu",
    /* 2 */  "vme",
    /* 3 */  "de",
    /* 4 */  "pse",
    /* 5 */  "tsc",
    /* 6 */  "msr",
    /* 7 */  "pae",
    /* 8 */  "mce",
    /* 9 */  "cx8",
    /* 10 */ "apic",
    /* 11 */ "sep",
    /* 12 */ "mtrr",
    /* 13 */ "pge",
    /* 14 */ "mca",
    /* 15 */ "cmov",
    /* 16 */ "pat",
    /* 17 */ "pse36",
    /* 18 */ "psn",
    /* 19 */ "clfsh",
    /* 20 */ "ds",
    /* 21 */ "acpi",
    /* 22 */ "mmx",
    /* 23 */ "fxsr",
    /* 24 */ "sse",
    /* 25 */ "sse2",
    /* 26 */ "ss",
    /* 27 */ "htt",
    /* 28 */ "tm",
    /* 29 */ "pbe",
    /* 30 */ "sse3",
    /* 31 */ "pclmulqdq",
    /* 32 */ "dtes64",
    /* 33 */ "mon",
    /* 34 */ "dscpl",
    /* 35 */ "vmx",
    /* 36 */ "smx",
    /* 37 */ "est",
    /* 38 */ "tm2",
    /* 39 */ "ssse3",
    /* 40 */ "cid",
    /* 41 */ "seglim64",
    /* 42 */ "fma",
    /* 43 */ "cx16",
    /* 44 */ "tpr",
    /* 45 */ "pdcm",
    /* 46 */ "pcid",
    /* 47 */ "dca",
    /* 48 */ "sse4.1",
    /* 49 */ "sse4.2",
    /* 50 */ "x2apic",
    /* 51 */ "movbe",
    /* 52 */ "popcnt",
    /* 53 */ "tsctmr",
    /* 54 */ "aes",
    /* 55 */ "xsave",
    /* 56 */ "osxsave",
    /* 57 */ "avx1.0",
    /* 58 */ "f16c",
    /* 59 */ "rdrand",
    /* 60 */ "vmm",
};

uint64_t feature_list[] = {
    /* 1 */  CPUID_FEATURE_FPU,
    /* 2 */  CPUID_FEATURE_VME,
    /* 3 */  CPUID_FEATURE_DE,
    /* 4 */  CPUID_FEATURE_PSE,
    /* 5 */  CPUID_FEATURE_TSC,
    /* 6 */  CPUID_FEATURE_MSR,
    /* 7 */  CPUID_FEATURE_PAE,
    /* 8 */  CPUID_FEATURE_MCE,
    /* 9 */  CPUID_FEATURE_CX8,
    /* 10 */ CPUID_FEATURE_APIC,
    /* 11 */ CPUID_FEATURE_SEP,
    /* 12 */ CPUID_FEATURE_MTRR,
    /* 13 */ CPUID_FEATURE_PGE,
    /* 14 */ CPUID_FEATURE_MCA,
    /* 15 */ CPUID_FEATURE_CMOV,
    /* 16 */ CPUID_FEATURE_PAT,
    /* 17 */ CPUID_FEATURE_PSE36,
    /* 18 */ CPUID_FEATURE_PSN,
    /* 19 */ CPUID_FEATURE_CLFSH,
    /* 20 */ CPUID_FEATURE_DS,
    /* 21 */ CPUID_FEATURE_ACPI,
    /* 22 */ CPUID_FEATURE_MMX,
    /* 23 */ CPUID_FEATURE_FXSR,
    /* 24 */ CPUID_FEATURE_SSE,
    /* 25 */ CPUID_FEATURE_SSE2,
    /* 26 */ CPUID_FEATURE_SS,
    /* 27 */ CPUID_FEATURE_HTT,
    /* 28 */ CPUID_FEATURE_TM,
    /* 29 */ CPUID_FEATURE_PBE,
    /* 30 */ CPUID_FEATURE_SSE3,
    /* 31 */ CPUID_FEATURE_PCLMULQDQ,
    /* 32 */ CPUID_FEATURE_DTES64,
    /* 33 */ CPUID_FEATURE_MONITOR,
    /* 34 */ CPUID_FEATURE_DSCPL,
    /* 35 */ CPUID_FEATURE_VMX,
    /* 36 */ CPUID_FEATURE_SMX,
    /* 37 */ CPUID_FEATURE_EST,
    /* 38 */ CPUID_FEATURE_TM2,
    /* 39 */ CPUID_FEATURE_SSSE3,
    /* 40 */ CPUID_FEATURE_CID,
    /* 41 */ CPUID_FEATURE_SEGLIM64,
    /* 42 */ CPUID_FEATURE_FMA,
    /* 43 */ CPUID_FEATURE_CX16,
    /* 44 */ CPUID_FEATURE_xTPR,
    /* 45 */ CPUID_FEATURE_PDCM,
    /* 46 */ CPUID_FEATURE_PCID,
    /* 47 */ CPUID_FEATURE_DCA,
    /* 48 */ CPUID_FEATURE_SSE4_1,
    /* 49 */ CPUID_FEATURE_SSE4_2,
    /* 50 */ CPUID_FEATURE_x2APIC,
    /* 51 */ CPUID_FEATURE_MOVBE,
    /* 52 */ CPUID_FEATURE_POPCNT,
    /* 53 */ CPUID_FEATURE_TSCTMR,
    /* 54 */ CPUID_FEATURE_AES,
    /* 55 */ CPUID_FEATURE_XSAVE,
    /* 56 */ CPUID_FEATURE_OSXSAVE,
    /* 57 */ CPUID_FEATURE_AVX1_0,
    /* 58 */ CPUID_FEATURE_F16C,
    /* 59 */ CPUID_FEATURE_RDRAND,
    /* 60 */ CPUID_FEATURE_VMM,
};

const char *feature_ext_flags[] = {
    /* 1 */ "syscall",
    /* 2 */ "xd",
    /* 3 */ "1gbpage",
    /* 4 */ "rdtscp",
    /* 5 */ "em64t",
    /* 6 */ "lahf",
    /* 7 */ "lzcnt",
    /* 8 */ "prefetchcw"
};

uint64_t feature_ext_list[] = {
    /* 1 */ CPUID_EXTFEATURE_SYSCALL,
    /* 2 */ CPUID_EXTFEATURE_XD,
    /* 3 */ CPUID_EXTFEATURE_1GBPAGE,
    /* 4 */ CPUID_EXTFEATURE_RDTSCP,
    /* 5 */ CPUID_EXTFEATURE_EM64T,
    /* 6 */ CPUID_EXTFEATURE_LAHF,
    /* 7 */ CPUID_EXTFEATURE_LZCNT,
    /* 8 */ CPUID_EXTFEATURE_PREFETCHW
};

const char *leaf7_feature_flags[] = {
    /* 1 */  "rdwrfsgs",
    /* 2 */  "tsc_thread_offset",
    /* 3 */  "sgx",
    /* 4 */  "bmi1",
    /* 5 */  "hle",
    /* 6 */  "avx2",
    /* 7 */  "fdpeo",
    /* 8 */  "smep",
    /* 9 */  "bmi2",
    /* 10 */ "erms",
    /* 11 */ "invpcid",
    /* 12 */ "rtm",
    /* 13 */ "pqm",
    /* 14 */ "fpu_csds",
    /* 15 */ "mpx",
    /* 16 */ "pqe",
    /* 17 */ "avx512f",
    /* 18 */ "avx512dq",
    /* 19 */ "rdseed",
    /* 20 */ "adx",
    /* 21 */ "smap",
    /* 22 */ "avx512ifma",
    /* 23 */ "clfsopt",
    /* 24 */ "clwb",
    /* 25 */ "ipt",
    /* 26 */ "avx512cd",
    /* 27 */ "sha",
    /* 28 */ "avx512bw",
    /* 29 */ "avx512vl",
    /* 30 */ "prefetchwt1",
    /* 31 */ "avx512vbmi",
    /* 32 */ "umip",
    /* 33 */ "pku",
    /* 34 */ "ospke",
    /* 35 */ "waitpkg",
    /* 36 */ "gfni",
    /* 37 */ "vaes",
    /* 38 */ "vpclmulqdq",
    /* 39 */ "avx512vnni",
    /* 40 */ "avx512bitalg",
    /* 41 */ "avx512vpopcntdq",
    /* 42 */ "rdpid",
    /* 43 */ "cldemote",
    /* 44 */ "movdiri",
    /* 45 */ "movdiri64b",
    /* 46 */ "sgxlc"
};

uint64_t leaf7_feature_list[] = {
    /* 1 */  CPUID_LEAF7_FEATURE_RDWRFSGS,
    /* 2 */  CPUID_LEAF7_FEATURE_TSCOFF,
    /* 3 */  CPUID_LEAF7_FEATURE_SGX,
    /* 4 */  CPUID_LEAF7_FEATURE_BMI1,
    /* 5 */  CPUID_LEAF7_FEATURE_HLE,
    /* 6 */  CPUID_LEAF7_FEATURE_AVX2,
    /* 7 */  CPUID_LEAF7_FEATURE_FDPEO,
    /* 8 */  CPUID_LEAF7_FEATURE_SMEP,
    /* 9 */  CPUID_LEAF7_FEATURE_BMI2,
    /* 10 */ CPUID_LEAF7_FEATURE_ERMS,
    /* 12 */ CPUID_LEAF7_FEATURE_INVPCID,
    /* 12 */ CPUID_LEAF7_FEATURE_RTM,
    /* 13 */ CPUID_LEAF7_FEATURE_PQM,
    /* 14 */ CPUID_LEAF7_FEATURE_FPU_CSDS,
    /* 15 */ CPUID_LEAF7_FEATURE_MPX,
    /* 16 */ CPUID_LEAF7_FEATURE_PQE,
    /* 17 */ CPUID_LEAF7_FEATURE_AVX512F,
    /* 18 */ CPUID_LEAF7_FEATURE_AVX512DQ,
    /* 19 */ CPUID_LEAF7_FEATURE_RDSEED,
    /* 20 */ CPUID_LEAF7_FEATURE_ADX,
    /* 21 */ CPUID_LEAF7_FEATURE_SMAP,
    /* 22 */ CPUID_LEAF7_FEATURE_AVX512IFMA,
    /* 23 */ CPUID_LEAF7_FEATURE_CLFSOPT,
    /* 23 */ CPUID_LEAF7_FEATURE_CLWB,
    /* 25 */ CPUID_LEAF7_FEATURE_IPT,
    /* 26 */ CPUID_LEAF7_FEATURE_AVX512CD,
    /* 27 */ CPUID_LEAF7_FEATURE_SHA,
    /* 28 */ CPUID_LEAF7_FEATURE_AVX512BW,
    /* 29 */ CPUID_LEAF7_FEATURE_AVX512VL,
    /* 30 */ CPUID_LEAF7_FEATURE_PREFETCHWT1,
    /* 31 */ CPUID_LEAF7_FEATURE_AVX512VBMI,
    /* 32 */ CPUID_LEAF7_FEATURE_UMIP,
    /* 33 */ CPUID_LEAF7_FEATURE_PKU,
    /* 34 */ CPUID_LEAF7_FEATURE_OSPKE,
    /* 35 */ CPUID_LEAF7_FEATURE_WAITPKG,
    /* 36 */ CPUID_LEAF7_FEATURE_GFNI,
    /* 37 */ CPUID_LEAF7_FEATURE_VAES,
    /* 38 */ CPUID_LEAF7_FEATURE_VPCLMULQDQ,
    /* 39 */ CPUID_LEAF7_FEATURE_AVX512VNNI,
    /* 40 */ CPUID_LEAF7_FEATURE_AVX512BITALG,
    /* 41 */ CPUID_LEAF7_FEATURE_AVX512VPCDQ,
    /* 42 */ CPUID_LEAF7_FEATURE_RDPID,
    /* 43 */ CPUID_LEAF7_FEATURE_CLDEMOTE,
    /* 44 */ CPUID_LEAF7_FEATURE_MOVDIRI,
    /* 45 */ CPUID_LEAF7_FEATURE_MOVDIRI64B,
    /* 46 */ CPUID_LEAF7_FEATURE_SGXLC
};

const char *leaf7_feature_ext_flags[] = {
    /* 1 */  "avx5124vnniw",
    /* 2 */  "avx5124fmaps",
    /* 3 */  "fsrepmov",
    /* 4 */  "srbds_ctlr",
    /* 5 */  "mdclear",
    /* 6 */  "tsxfa",
    /* 7 */  "ibrs",
    /* 8 */  "stibp",
    /* 9 */  "l1d",
    /* 10 */ "acapmsr",
    /* 11 */ "ccapmsr",
    /* 12 */ "ssbd"
};

uint64_t leaf7_feature_ext_list[] = {
    /* 1 */  CPUID_LEAF7_EXTFEATURE_AVX5124VNNIW,
    /* 2 */  CPUID_LEAF7_EXTFEATURE_AVX5124FMAPS,
    /* 3 */  CPUID_LEAF7_EXTFEATURE_FSREPMOV,
    /* 4 */  CPUID_LEAF7_EXTFEATURE_SRBDS_CTRL,
    /* 5 */  CPUID_LEAF7_EXTFEATURE_MDCLEAR,
    /* 6 */  CPUID_LEAF7_EXTFEATURE_TSXFA,
    /* 6 */  CPUID_LEAF7_EXTFEATURE_IBRS,
    /* 8 */  CPUID_LEAF7_EXTFEATURE_STIBP,
    /* 9 */  CPUID_LEAF7_EXTFEATURE_L1DF,
    /* 10 */ CPUID_LEAF7_EXTFEATURE_ACAPMSR,
    /* 11 */ CPUID_LEAF7_EXTFEATURE_CCAPMSR,
    /* 12 */ CPUID_LEAF7_EXTFEATURE_SSBD
};

const char *pm_flags[] = {
    /* 1 */  "ts",
    /* 2 */  "fid",
    /* 3 */  "vid",
    /* 4 */  "ttp",
    /* 5 */  "tm",
    /* 6 */  "stc",
    /* 7 */  "100mhzsteps",
    /* 8 */  "hwpstate",
    /* 9 */  "cpb",
    /* 10 */ "eff_freq_ro",
    /* 11 */ "proc_feedback",
    /* 12 */ "acc_power"
};

const char *bug_flags[] = {
    /* 1 */  "fxsave_leak",
    /* 2 */  "sysret_ss_attrs",
    /* 3 */  "null_seg",
    /* 4 */  "cpu_meltdown",
    /* 5 */  "spectre_v1",
    /* 6 */  "spectre_v2",
    /* 7 */  "spec_store_bypass",
    /* 8 */  "l1tf",
    /* 9 */  "mds",
    /* 10 */ "swapgs"
};

/*
 * Array sizes:
 *
 * sizeof(feature_flags)            = 480
 * sizeof(feature_ext_flags)        = 64
 * sizeof(leaf7_feature_flags)      = 368
 * sizeof(leaf7_feature_ext_flags)  = 96
 */

STATIC char *
get_cpu_flags(void)
{
    int i = 0;
    int size = 0;
    char *flags;
    static char *ret;

    size = sizeof(feature_flags);
    flags = _MALLOC(size, M_TEMP, M_WAITOK);

    do {
        /* 
         * If the CPU supports a feature in the feature_list[]...
         */
        if (_cpuid_info()->cpuid_features & feature_list[i]) {
            /*
             * ...amend its flag to 'flags'.
             */
            strlcat(flags, feature_flags[i], sizeof(flags));
        }

        ret = flags;

        /*
         * Add 1 to the counter for each iteration.
         */
        i++;

        /* 
         * If the counter exceeds the number of items in the array,
         * break the loop.
         */ 
        if (i > nitems(feature_flags)) {
            _FREE(&flags, M_TEMP);
            break;
        } else {
            continue;
        }
    } while (i < nitems(feature_flags));

    return ret;
}

STATIC char *
get_cpu_ext_flags(void)
{
    int i = 0;
    int size = 0;
    char *flags;
    static char *ret;

    size = sizeof(feature_ext_flags);
    flags = _MALLOC(size, M_TEMP, M_WAITOK);

    do {
        /* 
         * If the CPU supports a feature in the feature_ext_list[]...
         */
        if (_cpuid_info()->cpuid_extfeatures & feature_ext_list[i]) {
            /*
             * ...amend its flag to 'flags'.
             */
            strlcat(flags, feature_ext_flags[i], sizeof(flags));
        }

        ret = flags;

        /*
         * Add 1 to the counter for each iteration.
         */
        i++;

        /* 
         * If the counter exceeds the number of items in the array,
         * break the loop.
         */ 
        if (i > nitems(feature_ext_flags)) {
            _FREE(&flags, M_TEMP);
            break;
        } else {
            continue;
        }
    } while (i < nitems(feature_ext_flags));

    return ret;
}

STATIC char*
get_leaf7_flags(void)
{
    int i = 0;
    int size = 0;
    char *flags;
    static char *ret;

    size = sizeof(leaf7_feature_flags);
    flags = _MALLOC(size, M_TEMP, M_WAITOK);

    do {
        /* 
         * If the CPU supports a feature in the leaf7_feature_list[]...
         */
        if (_cpuid_info()->cpuid_leaf7_features & leaf7_feature_list[i]) {
            /*
             * ...amend its flag to 'flags'.
             */
            strlcat(flags, leaf7_feature_flags[i], sizeof(flags));
        }

        ret = flags;

        /*
         * Add 1 to the counter for each iteration.
         */
        i++;

        /* 
         * If the counter exceeds the number of items in the array,
         * break the loop.
         */ 
        if (i > nitems(leaf7_feature_flags)) {
            _FREE(&flags, M_TEMP);
            break;
        } else {
            continue;
        }
    } while (i < nitems(leaf7_feature_flags));

    return ret;
}

STATIC char *
get_leaf7_ext_flags(void)
{
    int i = 0;
    int size = 0;
    char *flags;
    static char *ret;

    size = sizeof(leaf7_feature_ext_flags);
    flags = _MALLOC(size, M_TEMP, M_WAITOK);

    do {
        /* 
         * If the CPU supports a feature in the leaf7_feature_ext_list[]...
         */
        if (_cpuid_info()->cpuid_leaf7_extfeatures & leaf7_feature_ext_list[i]) {
            /*
             * ...amend its flag to 'flags'.
             */
            strlcat(flags, leaf7_feature_ext_flags[i], sizeof(flags));
        }

        ret = flags;

        /*
         * Add 1 to the counter for each iteration.
         */
        i++;

        /* 
         * If the counter exceeds the number of items in the array,
         * break the loop.
         */ 
        if (i > nitems(leaf7_feature_ext_flags)) {
            _FREE(&flags, M_TEMP);
            break;
        } else {
            continue;
        }
    } while (i < nitems(leaf7_feature_ext_flags));

    return ret;
}
