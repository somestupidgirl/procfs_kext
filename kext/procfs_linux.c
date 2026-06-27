/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_linux.c
 *
 * Linux-compatible features.
 *
 * ARM64 support added 2024. On ARM64, procfs_docpuinfo() emits
 * output in Linux AArch64 /proc/cpuinfo format rather than the
 * x86 format, using hw.optional.arm.* sysctls and hw.cpufamily
 * for CPU identification.
 */
#include <stdint.h>
#include <string.h>
#if defined(__x86_64__)
#include <i386/cpuid.h>
#include <i386/tsc.h>
#else
#include <arm/cpuid.h>
#endif
#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>
#include <libkern/version.h>
#include <mach/machine.h>
#include <os/log.h>
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#if defined (__x86_64__)
#include <i386/proc_reg.h>
#else
#include <arm/proc_reg.h>
#endif
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <bsdcompat/sys/malloc.h>

#include <fs/procfs/procfs.h>

#include "lib/cpu.h"
#include "lib/symbols.h"

#pragma mark -
#pragma mark Common Definitions and Macros

/*
 * A buffer size.
 */
#define LBFSZ           (8 * 1024)

/*
 * Convert pages to bytes.
 */
#define PGTOB(p) \
        ((unsigned long)(p) << PAGE_SHIFT)
/*
 * Convert pages to kbytes
 */
#define PGTOKB(p) \
        ((unsigned long)(p) << (PAGE_SHIFT - 10))
/*
 * Convert bytes to pages.
 */
#define BTOPG(b) \
        ((b) >> PAGE_SHIFT)
/*
 * Convert bytes to kbytes
 */
#define BTOKB(b) \
        ((b) >> 10)
/*
 * XXX
 */
#define TVTOJ(x) \
        ((x)->tv_sec * 100UL + (x)->tv_usec / 10000)

/*
 * Various conversion macros
 */
#define T2J(x) ((long)(((x) * 100ULL) / (stathz ? stathz : hz)))    /* ticks to jiffies */
#define T2CS(x) ((unsigned long)(((x) * 100ULL) / (stathz ? stathz : hz)))  /* ticks to centiseconds */
#define T2S(x) ((x) / (stathz ? stathz : hz))       /* ticks to seconds */
#define B2K(x) ((x) >> 10)              /* bytes to kbytes */
#define B2P(x) ((x) >> PAGE_SHIFT)          /* bytes to pages */
#define P2B(x) ((x) << PAGE_SHIFT)          /* pages to bytes */
#define P2K(x) ((x) << (PAGE_SHIFT - 10))       /* pages to kbytes */
#define TV2J(x) ((x)->tv_sec * 100UL + (x)->tv_usec / 10000)

/**
 * @brief Mapping of ki_stat in struct kinfo_proc to the linux state
 *
 * The linux procfs state field displays one of the characters RSDZTW to
 * denote running, sleeping in an interruptible wait, waiting in an
 * uninterruptible disk sleep, a zombie process, process is being traced
 * or stopped, or process is paging respectively.
 *
 * Our struct kinfo_proc contains the variable ki_stat which contains a
 * value out of SIDL, SRUN, SSLEEP, SSTOP, SZOMB, SWAIT and SLOCK.
 *
 * This character array is used with ki_stati-1 as an index and tries to
 * map our states to suitable linux states.
 */
static char linux_state[] = "RRSTZDD";

#pragma mark -
#pragma mark Linux-emulation functions

/*
 * Linux-compatible /proc/cpuinfo
 */
int
procfs_docpuinfo(__unused pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int len = 0, xlen = 0;
    vm_offset_t pageno, uva;
    off_t page_offset = 0;
    size_t buffer_size = 0;
    char *buffer;

    /*
     * Overall processor count for the current CPU.
     *
     * Not to be conflated with cpu_cores (number of cores)
     * as these are not the same.
     */
    /* processor_count is a private symbol - use sysctlbyname instead */
    uint32_t max_cpus = 1;
    if (_processor_count != NULL) {
        max_cpus = processor_count;
    } else {
        size_t max_cpus_size = sizeof(max_cpus);
        sysctlbyname("hw.logicalcpu", &max_cpus, &max_cpus_size, NULL, 0);
    }
    uint32_t cnt_cpus = 0;

    /*
     * Initialize the processor counter.
     * This should always begin at 0 and
     * add 1 for each loop according to the
     * number of processors present.
     */
    buffer_size = (LBFSZ * 4);
    buffer = malloc(buffer_size, M_TEMP, M_WAITOK);

    /*
     * Get the userspace virtual address.
     */
    uva = uio_offset(uio);

    /*
     * Get the page number of this segment.
     */
    pageno = trunc_page(uva);
    page_offset = uva - pageno;

/* ============================================================
 * x86_64 /proc/cpuinfo
 * ============================================================ */
#if defined(__x86_64__)

    /*
     * Initialize the TSC frequency variables.
     */
    uint64_t freq = tscFreq;
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
     * The core id should always start at 0.
     */
    int core_id = 0;

    /*
     * Here we can utilize the i386_cpu_info structure in i386/cpuid.h
     * to get the information we need. The cpuid_info() function sets up
     * the i386_cpu_info structure and returns a pointer to the structure.
     */
    char *vendor_id      = cpuid_info()->cpuid_vendor;
    uint8_t cpu_family   = cpuid_info()->cpuid_family;
    uint8_t model        = cpuid_info()->cpuid_model
                         + (cpuid_info()->cpuid_extmodel << 4);
    char *model_name     = cpuid_info()->cpuid_brand_string;
    int microcode        = (int)get_microcode_version();
    uint32_t cache_size  = cpuid_info()->cpuid_cache_size;
    uint8_t stepping     = cpuid_info()->cpuid_stepping;
    uint32_t cpu_cores   = cpuid_info()->core_count;
    uint32_t cpuid_level = cpu_cores;
    uint32_t tlb_size    = cpuid_info()->cache_linesize * 40;
    uint32_t clflush_size    = cpuid_info()->cache_linesize;
    uint32_t cache_alignment = clflush_size;
    uint32_t addr_bits_phys  = cpuid_info()->cpuid_address_bits_physical;
    uint32_t addr_bits_virt  = cpuid_info()->cpuid_address_bits_virtual;

    /*
     * Check if the FPU feature is present.
     */
    char *fpu, *fpu_exception;
    if (cpuid_info()->cpuid_features & CPUID_FEATURE_FPU) {
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
    char *pm = "";
    char *x86_bugs = "";
    char *x86_64_bugs = "";

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
        cpuflags     = get_cpu_flags();
        cpuextflags  = get_cpu_ext_flags();
        leaf7flags   = get_leaf7_flags();
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
                "microcode\t\t: 0x%07x\n"
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
                cnt_cpus,
                vendor_id,
                cpu_family,
                model,
                model_name,
                microcode,
                stepping,
                fqmhz, fqkhz,
                cache_size,
                0,
                max_cpus,
                core_id,
                cpu_cores,
                apicid,
                initial_apicid,
                fpu,
                fpu_exception,
                cpuid_level,
                wp,
                cpuflags, cpuextflags, leaf7flags, leaf7extflags,
                x86_bugs, x86_64_bugs,
                fqmhz * 2, fqkhz,
                tlb_size,
                clflush_size,
                cache_alignment,
                addr_bits_phys,
                addr_bits_virt,
                pm
            );

            /*
             * How many bytes to copy.
             */
            xlen = (len - page_offset);

            /*
             * Do the move into userspace.
             */
            uiomove((const char *)buffer, xlen, uio);

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
            if ((int)apicid >= (int)max_cpus) {
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
            if ((int)core_id > (int)cpu_cores - 1) {
                core_id = 0;
            }
            if (_processor_count != NULL && max_cpus != processor_count) {
                max_cpus = processor_count;
            }
        } else if (cnt_cpus > max_cpus) {
            break;
        }
    }

/* ============================================================
 * ARM64 /proc/cpuinfo
 *
 * Produces one entry per logical CPU in Linux AArch64 format:
 *
 *   processor       : 0
 *   BogoMIPS        : 48.00
 *   Features        : fp asimd aes pmull sha1 sha2 crc32 ...
 *   CPU implementer : 0x61
 *   CPU architecture: 8
 *   CPU variant     : 0x0
 *   CPU part        : 0x22
 *   CPU revision    : 0
 *
 * ============================================================ */
#elif defined(__arm64__) || defined(__aarch64__)

    const char *bogomips     = arm64_bogomips();
    const char *features     = get_cpu_flags();
    const char *implementer  = arm64_cpu_implementer();
    const char *architecture = arm64_cpu_architecture();
    const char *variant      = arm64_cpu_variant();
    const char *part         = arm64_cpu_part();
    const char *revision     = arm64_cpu_revision();

    while (cnt_cpus < max_cpus) {
        if (cnt_cpus <= max_cpus) {
            len += snprintf(buffer, buffer_size,
                "processor\t\t: %u\n"
                "BogoMIPS\t\t: %s\n"
                "Features\t\t: %s\n"
                "CPU implementer\t\t: %s\n"
                "CPU architecture\t: %s\n"
                "CPU variant\t\t: %s\n"
                "CPU part\t\t: %s\n"
                "CPU revision\t\t: %s\n\n",
                cnt_cpus,
                bogomips,
                features,
                implementer,
                architecture,
                variant,
                part,
                revision
            );

            xlen = (len - page_offset);
            uiomove((const char *)buffer, xlen, uio);

            if (len != 0) len = 0;

            cnt_cpus++;

            /*
             * Reset the max_cpus variable at the end of each loop.
             * An unknown bug is causing the variable to reduce the
             * count down to 23 unless we do this.
             */
            if (_processor_count != NULL && max_cpus != processor_count) {
                max_cpus = processor_count;
            }
        } else if (cnt_cpus > max_cpus) {
            /*
             * If the counter exceeds the processor count,
             * break the loop.
             */
            break;
        }
    }

#endif /* __x86_64__ / __arm64__ */

    free(buffer, M_TEMP);

    return 0;
}

/*
 * Linux-compatible /proc/loadavg
 */
int
procfs_doloadavg(__unused pfsnode_t *pnp, uio_t uio, vfs_context_t ctx)
{
    int error = 0;
    int len = 0, xlen = 0;
    vm_offset_t off = uio_offset(uio);
    vm_offset_t pgno = trunc_page(off);
    off_t pgoff = (off - pgno);

    char *buf = malloc(LBFSZ, M_TEMP, M_WAITOK);

    // Report the load averages from our local averunnable (lib/kern.c). The
    // kernel's own averunnable is not reachable from a kext, and there is
    // currently no driver populating ours: every CPU-load / run-queue source is
    // either unexported (host_statistics, processor_set_statistics) or in the
    // Apple-only com.apple.kpi.private KPI (cpu_to_processor + processor_info).
    // So these values read 0.00 until a usable nrun source is found.
    long fscale = averunnable.fscale > 0 ? averunnable.fscale : FSCALE;
    int load1  = (int)((uint64_t)averunnable.ldavg[0] * 100 / fscale);
    int load5  = (int)((uint64_t)averunnable.ldavg[1] * 100 / fscale);
    int load15 = (int)((uint64_t)averunnable.ldavg[2] * 100 / fscale);

    int total_procs = procfs_get_process_count(vfs_context_ucred(ctx));
    int running = 1;
    int lastpid = 0;

    len = snprintf(buf, LBFSZ,
        "%d.%02d %d.%02d %d.%02d %d/%d %d\n",
        load1  / 100, load1  % 100,
        load5  / 100, load5  % 100,
        load15 / 100, load15 % 100,
        running,
        total_procs,
        lastpid
    );

    xlen = (len - pgoff);
    error = uiomove((const char *)buf, xlen, uio);

    free(buf, M_TEMP);

    return error;
}

/*
 * Linux-compatible /proc/partitions
 */
int
procfs_dopartitions(__unused pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;
    int len = 0, xlen = 0;

    vm_offset_t off = uio_offset(uio);
    vm_offset_t pgno = trunc_page(off);
    off_t pgoff = (off - pgno);

    int major = 0, minor = 0, block_size = 0;
    char *name = "Feature not yet implemented.";

    char *buf = malloc(LBFSZ, M_TEMP, M_WAITOK);

    len = snprintf(buf, LBFSZ, "major minor  #blocks  name\n"
                               "%d %d %d %s\n",
                               major, minor, block_size, name);

    xlen = (len - pgoff);
    error = uiomove((const char *)buf, xlen, uio);

    free(buf, M_TEMP);

    return error;
}

/*
 * Linux-compatible /proc/version
 */
int
procfs_doversion(__unused pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;
    int len = 0, xlen = 0;

    vm_offset_t off = uio_offset(uio);
    vm_offset_t pgno = trunc_page(off);
    off_t pgoff = (off - pgno);

    char *buf = malloc(LBFSZ, M_TEMP, M_WAITOK);

    /*
     * Print out the kernel version string.
     */
    len = snprintf(buf, LBFSZ, "%s\n", version);

    xlen = (len - pgoff);
    error = uiomove((const char *)buf, xlen, uio);

    free(buf, M_TEMP);

    return error;
}