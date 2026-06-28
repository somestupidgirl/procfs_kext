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
#include <kern/clock.h>
#include <kern/thread_call.h>
#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>
#include <libkern/version.h>
#include <mach/machine.h>
#include <mach/mach_types.h>
#include <mach/processor_info.h>
#include <os/log.h>
#include <ptrauth.h>
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#if defined (__x86_64__)
#include <i386/proc_reg.h>
#else
#include <arm/proc_reg.h>
#endif
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#if defined(__x86_64__)
#include <mach/i386/vm_param.h>
#else
#include <mach/vm_param.h>
#endif

#include <bsdcompat/sys/malloc.h>

#include <fs/procfs/procfs.h>
#include "lib/symbols.h"

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
 * Load-average sampler for the loadavg node.
 *
 * A true run-queue load average is unreachable on arm64: averunnable,
 * compute_averunnable, host_statistics and processor_set_info are all stripped
 * from the kernel and unexported. What IS reachable is per-CPU tick counts via
 * processor_info(PROCESSOR_CPU_LOAD_INFO), given a processor_t from
 * cpu_to_processor() (resolved through libklookup). So we periodically sample
 * CPU utilisation and feed util*ncpu through the standard load-average EWMA,
 * writing the result into the local `averunnable` (lib/kern.c) the node reads.
 *
 * NOTE: this approximates CPU utilisation (it saturates near ncpu), not the
 * true run-queue length, so it under-reports an overloaded machine. It is a
 * best-effort stand-in for the otherwise-unavailable load average.
 */
extern kern_return_t processor_info(processor_t processor, processor_flavor_t flavor,
    host_t *host, processor_info_t info, mach_msg_type_number_t *count);

typedef processor_t (*cpu_to_processor_fn)(int);

#define LA_SAMPLE_SECS 5

/* exp(-5s/{60,300,900}s) * FSCALE - the classic 5-second-sample decay table. */
static const fixpt_t la_cexp[3] = { 1884, 2014, 2037 };

static thread_call_t        la_call;
static cpu_to_processor_fn  la_cpu_to_processor;
static int                  la_ncpu;
static uint64_t             la_busy;        /* cumulative across CPUs, last sample */
static uint64_t             la_idle;
static boolean_t            la_primed;

/*
 * Sum busy/idle ticks across all CPUs, then fold a utilisation-derived
 * instantaneous load into the three EWMAs.
 */
static void
la_sample(void)
{
    uint64_t busy = 0, idle = 0;

    for (int i = 0; i < la_ncpu; i++) {
        processor_t pr = la_cpu_to_processor(i);
        if (pr == PROCESSOR_NULL) {
            continue;
        }
        processor_cpu_load_info_data_t info;
        mach_msg_type_number_t count = PROCESSOR_CPU_LOAD_INFO_COUNT;
        host_t host;
        if (processor_info(pr, PROCESSOR_CPU_LOAD_INFO, &host,
                (processor_info_t)&info, &count) != KERN_SUCCESS) {
            continue;
        }
        busy += (uint64_t)info.cpu_ticks[CPU_STATE_USER] +
                info.cpu_ticks[CPU_STATE_SYSTEM] +
                info.cpu_ticks[CPU_STATE_NICE];
        idle += info.cpu_ticks[CPU_STATE_IDLE];
    }

    if (!la_primed) {
        la_busy = busy;
        la_idle = idle;
        la_primed = TRUE;
        return;
    }

    uint64_t dbusy = busy - la_busy;
    uint64_t didle = idle - la_idle;
    la_busy = busy;
    la_idle = idle;

    /* Instantaneous "load" = utilisation * ncpu, in fixed point. */
    uint64_t total = dbusy + didle;
    fixpt_t nrun = 0;
    if (total > 0) {
        nrun = (fixpt_t)((dbusy * (uint64_t)la_ncpu * FSCALE) / total);
    }

    for (int i = 0; i < 3; i++) {
        averunnable.ldavg[i] = (fixpt_t)(((uint64_t)la_cexp[i] * averunnable.ldavg[i] +
            (uint64_t)nrun * (FSCALE - la_cexp[i])) >> FSHIFT);
    }
    averunnable.fscale = FSCALE;
}

static void
la_timer(__unused thread_call_param_t a, __unused thread_call_param_t b)
{
    uint64_t deadline;

    la_sample();
    clock_interval_to_deadline(LA_SAMPLE_SECS, NSEC_PER_SEC, &deadline);
    thread_call_enter_delayed(la_call, deadline);
}

/*
 * Start the periodic sampler. No-op (loadavg values stay zero) if libklookup
 * could not resolve cpu_to_processor.
 */
void
procfs_loadavg_start(void)
{
    if (la_call != NULL || procfs_kl_cpu_to_processor == NULL) {
        return;
    }

    /* Sign the klookup-resolved address for the arm64e ABI (disc 0; the
     * assignment resigns to the call's type discriminator). */
    la_cpu_to_processor = ptrauth_sign_unauthenticated(procfs_kl_cpu_to_processor,
        ptrauth_key_function_pointer, 0);

    int ncpu = 0;
    size_t sz = sizeof(ncpu);
    if (sysctlbyname("hw.logicalcpu", &ncpu, &sz, NULL, 0) != 0 || ncpu <= 0) {
        ncpu = 1;
    }
    la_ncpu = ncpu;
    averunnable.fscale = FSCALE;

    la_call = thread_call_allocate(la_timer, NULL);
    if (la_call == NULL) {
        return;
    }

    la_sample();    /* prime the cumulative counters */

    uint64_t deadline;
    clock_interval_to_deadline(LA_SAMPLE_SECS, NSEC_PER_SEC, &deadline);
    thread_call_enter_delayed(la_call, deadline);
}

/*
 * Stop the sampler (kext unload).
 */
void
procfs_loadavg_stop(void)
{
    if (la_call != NULL) {
        thread_call_cancel(la_call);
        thread_call_free(la_call);
        la_call = NULL;
    }
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

    // Report the load averages from our local averunnable (lib/kern.c), which
    // procfs_loadavg_start()'s sampler keeps populated from per-CPU utilisation
    // (the kernel's own averunnable and every run-queue source are stripped on
    // arm64). When libklookup can't resolve cpu_to_processor the sampler never
    // runs and these stay 0.00.
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
 * Linux-compatible /proc/meminfo, modelled on FreeBSD's linprocfs_domeminfo()
 * (sys/compat/linux/linprocfs/linprocfs.c) - same field set and "%9lu kB"
 * layout, and the same "all memory that isn't wired down is free" estimate
 * (FreeBSD: memused = vm_wire_count * PAGE_SIZE; memfree = memtotal - memused).
 *
 * Data sources differ on macOS. MemTotal comes from the hw.memsize sysctl
 * (readable from kernel context). The wired-page count comes from the kernel's
 * vm_page_wire_count global, resolved through libklookup (it survives in the
 * arm64 kernel symbol table; the vm.* page-count sysctls are not readable from
 * kernel context, and most vm_page_*_count globals are stripped). Cached,
 * Buffers and swap have no comparable source on arm64 and are reported as 0
 * (Buffers is 0 on FreeBSD too). If libklookup did not resolve the wired count,
 * MemFree is reported as 0 rather than guessed.
 */
int
procfs_domeminfo(__unused pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    uint64_t memtotal = 0;                       /* total memory in bytes */
    size_t   sz = sizeof(memtotal);
    (void)sysctlbyname("hw.memsize", &memtotal, &sz, NULL, 0);

    unsigned long memfree = 0;
    if (procfs_vm_page_wire_count != NULL) {
        uint64_t wired = (uint64_t)*procfs_vm_page_wire_count * PAGE_SIZE;
        memfree = (unsigned long)(memtotal > wired ? memtotal - wired : 0);
    }

    unsigned long      cached    = 0;
    unsigned long      buffers   = 0;
    unsigned long long swaptotal = 0;
    unsigned long long swapfree  = 0;

    char buf[512];
    struct sbuf sb;
    if (sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN) == NULL) {
        return ENOMEM;
    }

    sbuf_printf(&sb,
        "MemTotal: %9lu kB\n"
        "MemFree:  %9lu kB\n"
        "MemShared:%9lu kB\n"
        "Buffers:  %9lu kB\n"
        "Cached:   %9lu kB\n"
        "SwapTotal:%9llu kB\n"
        "SwapFree: %9llu kB\n",
        (unsigned long)B2K(memtotal), (unsigned long)B2K(memfree), 0UL,
        (unsigned long)B2K(buffers), (unsigned long)B2K(cached),
        (unsigned long long)B2K(swaptotal), (unsigned long long)B2K(swapfree));
    sbuf_finish(&sb);

    int error = procfs_copy_data(sbuf_data(&sb), sbuf_len(&sb), uio);
    sbuf_delete(&sb);

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