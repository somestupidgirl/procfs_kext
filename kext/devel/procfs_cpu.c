/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2000 Dag-Erling Coïdan Smørgrav
 * Copyright (c) 1999 Pierre Beyssac
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)procfs_status.c 8.4 (Berkeley) 6/15/94
 */
 /*
  * procfs_cpu.c
  *
  * Based on FreeBSD's linprocfs_docpuinfo() function.
  */
#include <string.h>
#include <i386/cpu_number.h>
#include <i386/cpuid.h>
#include <i386/tsc.h>
#include <libkern/libkern.h>
#include <mach/machine.h>
#include <sys/errno.h>
#include <sys/sbuf.h>

#include <miscfs/procfs/procfs.h>
#include <libklookup/klookup.h>

#ifndef CPUID_MODEL_COMETLAKE
#define CPUID_MODEL_COMETLAKE 0xA5
#endif

STATIC boolean_t
procfs_is_amd_cpu(void)
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

STATIC boolean_t 
procfs_is_intel_cpu(void)
{
    uint32_t ourcpuid[4];
    do_cpuid(0, ourcpuid);
    if (ourcpuid[ebx] == 0x756E6547 &&
        ourcpuid[ecx] == 0x6C65746E &&
        ourcpuid[edx] == 0x49656E69) {
        return TRUE;
    }

    if (!procfs_is_amd_cpu()) {
        return TRUE;
    }

    return FALSE;
}

STATIC char
procfs_cpuid_vendor(void) {
    char cpuvendor = '\0';

    boolean_t amd = procfs_is_amd_cpu();
    boolean_t intel = procfs_is_intel_cpu();

    if (intel == TRUE && amd == FALSE) {
        cpuvendor = CPUID_VID_INTEL;
    } else if (amd == TRUE && intel == FALSE) {
        cpuvendor = CPUID_VID_AMD;
    } else {
        cpuvendor = CPUID_STRING_UNKNOWN;
    }

    return cpuvendor;
}

uint32_t (*_cpuid_family)(void);

STATIC uint32_t
procfs_cpuid_cpufamily(void)
{
    uint32_t cpufamily = CPUFAMILY_UNKNOWN;

    switch (cpuid_info()->cpuid_family) {
    case 6:
        switch (cpuid_info()->cpuid_model) {
        case 23:
            cpufamily = CPUFAMILY_INTEL_PENRYN;
            break;
        case CPUID_MODEL_NEHALEM:
        case CPUID_MODEL_FIELDS:
        case CPUID_MODEL_DALES:
        case CPUID_MODEL_NEHALEM_EX:
            cpufamily = CPUFAMILY_INTEL_NEHALEM;
            break;
        case CPUID_MODEL_DALES_32NM:
        case CPUID_MODEL_WESTMERE:
        case CPUID_MODEL_WESTMERE_EX:
            cpufamily = CPUFAMILY_INTEL_WESTMERE;
            break;
        case CPUID_MODEL_SANDYBRIDGE:
        case CPUID_MODEL_JAKETOWN:
            cpufamily = CPUFAMILY_INTEL_SANDYBRIDGE;
            break;
        case CPUID_MODEL_IVYBRIDGE:
        case CPUID_MODEL_IVYBRIDGE_EP:
            cpufamily = CPUFAMILY_INTEL_IVYBRIDGE;
            break;
        case CPUID_MODEL_HASWELL:
        case CPUID_MODEL_HASWELL_EP:
        case CPUID_MODEL_HASWELL_ULT:
        case CPUID_MODEL_CRYSTALWELL:
            cpufamily = CPUFAMILY_INTEL_HASWELL;
            break;
        case CPUID_MODEL_BROADWELL:
        case CPUID_MODEL_BRYSTALWELL:
            cpufamily = CPUFAMILY_INTEL_BROADWELL;
            break;
        case CPUID_MODEL_SKYLAKE:
        case CPUID_MODEL_SKYLAKE_DT:
        case CPUID_MODEL_SKYLAKE_W:
            cpufamily = CPUFAMILY_INTEL_SKYLAKE;
            break;
        case CPUID_MODEL_KABYLAKE:
        case CPUID_MODEL_KABYLAKE_DT:
            cpufamily = CPUFAMILY_INTEL_KABYLAKE;
            break;
        case CPUID_MODEL_ICELAKE:
        case CPUID_MODEL_ICELAKE_H:
        case CPUID_MODEL_ICELAKE_DT:
            cpufamily = CPUFAMILY_INTEL_ICELAKE;
            break;
        case CPUID_MODEL_COMETLAKE:
            cpufamily = CPUFAMILY_INTEL_COMETLAKE;
            break;
        }
        break;
    }

    return cpufamily;
}

STATIC uint32_t
procfs_cpu_model(void)
{
    uint32_t cpumodel = 0;
    uint32_t cpufamily = procfs_cpuid_cpufamily();

    if (cpufamily == CPUFAMILY_INTEL_PENRYN) {
        cpumodel = CPUID_MODEL_PENRYN;
    } else if (cpufamily == CPUFAMILY_INTEL_NEHALEM) {
        cpumodel = CPUID_MODEL_NEHALEM;
    } else if (cpufamily == CPUFAMILY_INTEL_WESTMERE) {
        cpumodel = CPUID_MODEL_WESTMERE;
    } else if (cpufamily == CPUFAMILY_INTEL_SANDYBRIDGE) {
        cpumodel = CPUID_MODEL_SANDYBRIDGE;
    } else if (cpufamily == CPUFAMILY_INTEL_IVYBRIDGE) {
        cpumodel = CPUID_MODEL_IVYBRIDGE;
    } else if (cpufamily == CPUFAMILY_INTEL_HASWELL) {
        cpumodel = CPUID_MODEL_HASWELL;
    } else if (cpufamily == CPUFAMILY_INTEL_BROADWELL) {
        cpumodel = CPUID_MODEL_BROADWELL;
    } else if (cpufamily == CPUFAMILY_INTEL_SKYLAKE) {
        cpumodel = CPUID_MODEL_SKYLAKE;
    } else if (cpufamily == CPUFAMILY_INTEL_KABYLAKE) {
        cpumodel = CPUID_MODEL_KABYLAKE;
    } else if (cpufamily == CPUFAMILY_INTEL_ICELAKE) {
        cpumodel = CPUID_MODEL_ICELAKE;
    } else if (cpufamily == CPUFAMILY_INTEL_COMETLAKE) {
        cpumodel = CPUID_MODEL_COMETLAKE;
    }

    return cpumodel;
}

#define nitems(x)   (sizeof((x)) / sizeof((x)[0]))
#define CPUID_FPU   0x00000001

int
procfs_cpuinfo(void)
{
    __typeof(tscFreq) _tscFreq = SymbolLookup("_tscFreq");
    struct sbuf sb;
    //int hw_model[2];
    //char model[128];
    uint64_t freq;
    size_t size;
    u_int cache_size[4];
    int fqmhz, fqkhz;
    int i, j;

    /*
     * We default the flags to include all non-conflicting flags,
     * and the Intel versions of conflicting flags.
     */
    static char *cpu_feature_names[] = {
        /*  0 */ "fpu", "vme", "de", "pse",
        /*  4 */ "tsc", "msr", "pae", "mce",
        /*  8 */ "cx8", "apic", "", "sep",
        /* 12 */ "mtrr", "pge", "mca", "cmov",
        /* 16 */ "pat", "pse36", "pn", "clflush",
        /* 20 */ "", "dts", "acpi", "mmx",
        /* 24 */ "fxsr", "sse", "sse2", "ss",
        /* 28 */ "ht", "tm", "ia64", "pbe"
    };

    static char *amd_feature_names[] = {
        /*  0 */ "", "", "", "",
        /*  4 */ "", "", "", "",
        /*  8 */ "", "", "", "syscall",
        /* 12 */ "", "", "", "",
        /* 16 */ "", "", "", "mp",
        /* 20 */ "nx", "", "mmxext", "",
        /* 24 */ "", "fxsr_opt", "pdpe1gb", "rdtscp",
        /* 28 */ "", "lm", "3dnowext", "3dnow"
    };

    static char *cpu_feature2_names[] = {
        /*  0 */ "pni", "pclmulqdq", "dtes64", "monitor",
        /*  4 */ "ds_cpl", "vmx", "smx", "est",
        /*  8 */ "tm2", "ssse3", "cid", "sdbg",
        /* 12 */ "fma", "cx16", "xtpr", "pdcm",
        /* 16 */ "", "pcid", "dca", "sse4_1",
        /* 20 */ "sse4_2", "x2apic", "movbe", "popcnt",
        /* 24 */ "tsc_deadline_timer", "aes", "xsave", "",
        /* 28 */ "avx", "f16c", "rdrand", "hypervisor"
    };

    static char *amd_feature2_names[] = {
        /*  0 */ "lahf_lm", "cmp_legacy", "svm", "extapic",
        /*  4 */ "cr8_legacy", "abm", "sse4a", "misalignsse",
        /*  8 */ "3dnowprefetch", "osvw", "ibs", "xop",
        /* 12 */ "skinit", "wdt", "", "lwp",
        /* 16 */ "fma4", "tce", "", "nodeid_msr",
        /* 20 */ "", "tbm", "topoext", "perfctr_core",
        /* 24 */ "perfctr_nb", "", "bpext", "ptsc",
        /* 28 */ "perfctr_llc", "mwaitx", "", ""
    };

    static char *cpu_stdext_feature_names[] = {
        /*  0 */ "fsgsbase", "tsc_adjust", "", "bmi1",
        /*  4 */ "hle", "avx2", "", "smep",
        /*  8 */ "bmi2", "erms", "invpcid", "rtm",
        /* 12 */ "cqm", "", "mpx", "rdt_a",
        /* 16 */ "avx512f", "avx512dq", "rdseed", "adx",
        /* 20 */ "smap", "avx512ifma", "", "clflushopt",
        /* 24 */ "clwb", "intel_pt", "avx512pf", "avx512er",
        /* 28 */ "avx512cd", "sha_ni", "avx512bw", "avx512vl"
    };

    static char *power_flags[] = {
        "ts",           "fid",          "vid",
        "ttp",          "tm",           "stc",
        "100mhzsteps",  "hwpstate",     "",
        "cpb",          "eff_freq_ro",  "proc_feedback",
        "acc_power",
    };

    // FreeBSD: int mp_ncpus us a global vatiable holding the
    // number of CPUs on a multi processor (MP) CPU.
    //
    // Examples of usage:
    // https://cpp.hotexamples.com/examples/-/-/CPU_SETOF/cpp-cpu_setof-function-examples.html
    //
    // XNU's processor_count global variable may represent
    // something similar but we also have a public KPI function
    // lurking in i386/cpu_number.h that seems to serve the same
    // purpose (I think).
    int mp_ncpus = cpu_number();

    uint32_t cpu_family = procfs_cpuid_cpufamily(); // cpuid_info()->cpuid_family
    uint32_t cpu_model = procfs_cpu_model(); // cpuid_info()->cpuid_model
    char cpu_model_name; // cpuid_info()->cpuid_brand_string
    char cpu_vendor = procfs_cpuid_vendor(); // cpuid_info()->cpuid_vendor
    uint32_t cpu_stepping; // cpuid_info()->cpuid_stepping
    uint32_t cpu_level = cpu_family; // These are set to the same value in FreeBSD
    u_int cpu_clflush_line_size = 32; // What it's set to in FreeBSD, I suppose.
    u_int cpu_maxphyaddr; // cpuid_info()->cpuid_address_bits_physical ?

    // I have no idea how to define these as of yet:
    u_int cpu_exthigh;
    u_int cpu_feature;
    u_int amd_feature;
    u_int cpu_feature2;
    u_int amd_feature2;
    u_int cpu_stdext_feature;
    u_int amd_pminfo;

    if (cpu_exthigh >= 0x80000006)
        do_cpuid(0x80000006, cache_size);
    else
        memset(cache_size, 0, sizeof(cache_size));
    for (i = 0; i < mp_ncpus; ++i) {
        fqmhz = 0;
        fqkhz = 0;
        freq = _tscFreq;
        if (freq != 0) {
            fqmhz = (freq + 4999) / 1000000;
            fqkhz = ((freq + 4999) / 10000) % 100;
        }
        sbuf_printf(&sb,
            "processor\t: %d\n"
            "vendor_id\t: %.20s\n"
            "cpu family\t: %u\n"
            "model\t\t: %u\n"
            "model name\t: %s\n"
            "stepping\t: %u\n"
            "cpu MHz\t\t: %d.%02d\n"
            "cache size\t: %d KB\n"
            "physical id\t: %d\n"
            "siblings\t: %d\n"
            "core id\t\t: %d\n"
            "cpu cores\t: %d\n"
            "apicid\t\t: %d\n"
            "initial apicid\t: %d\n"
            "fpu\t\t: %s\n"
            "fpu_exception\t: %s\n"
            "cpuid level\t: %d\n"
            "wp\t\t: %s\n",
            i, cpu_vendor, cpu_family, cpu_model, cpu_model_name, cpu_stepping,
            fqmhz, fqkhz, (cache_size[2] >> 16), 0, mp_ncpus, i, mp_ncpus, i, i,
            (cpu_feature & CPUID_FPU) ? "yes" : "no", "yes", cpu_level, "yes");
        sbuf_cat(&sb, "flags\t\t:");
        for (j = 0; j < nitems(cpu_feature_names); j++)
            if (cpu_feature & (1 << j) &&
                cpu_feature_names[j][0] != '\0')
                sbuf_printf(&sb, " %s", cpu_feature_names[j]);
        for (j = 0; j < nitems(amd_feature_names); j++)
            if (amd_feature & (1 << j) &&
                amd_feature_names[j][0] != '\0')
                sbuf_printf(&sb, " %s", amd_feature_names[j]);
        for (j = 0; j < nitems(cpu_feature2_names); j++)
            if (cpu_feature2 & (1 << j) &&
                cpu_feature2_names[j][0] != '\0')
                sbuf_printf(&sb, " %s", cpu_feature2_names[j]);
        for (j = 0; j < nitems(amd_feature2_names); j++)
            if (amd_feature2 & (1 << j) &&
                amd_feature2_names[j][0] != '\0')
                sbuf_printf(&sb, " %s", amd_feature2_names[j]);
        for (j = 0; j < nitems(cpu_stdext_feature_names); j++)
            if (cpu_stdext_feature & (1 << j) &&
                cpu_stdext_feature_names[j][0] != '\0')
                sbuf_printf(&sb, " %s",
                    cpu_stdext_feature_names[j]);
        sbuf_cat(&sb, "\n");
        sbuf_printf(&sb,
            "bugs\t\t: %s\n"
            "bogomips\t: %d.%02d\n"
            "clflush size\t: %d\n"
            "cache_alignment\t: %d\n"
            "address sizes\t: %d bits physical, %d bits virtual\n",
            "",
            fqmhz * 2, fqkhz,
            cpu_clflush_line_size, cpu_clflush_line_size,
            cpu_maxphyaddr,
            (cpu_maxphyaddr > 32) ? 48 : 0);
        sbuf_cat(&sb, "power management: ");
        for (j = 0; j < nitems(power_flags); j++)
            if (amd_pminfo & (1 << j))
                sbuf_printf(&sb, " %s", power_flags[j]);
        sbuf_cat(&sb, "\n\n");

        /* XXX per-cpu vendor / class / model / id? */
    }
    sbuf_cat(&sb, "\n");
}
