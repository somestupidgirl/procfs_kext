/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * procfs_linux.c
 *
 * Linux-compatible features.
 */
#include <stdint.h>
#include <string.h>
#include <i386/cpuid.h>
#include <i386/tsc.h>
#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>
#include <libkern/version.h>
#include <mach/machine.h>
#include <os/log.h>
//#include <sys/disklabel.h>
//#include <sys/disktab.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/proc_reg.h>
#include <sys/queue.h>
//#include <sys/sdt_impl.h>
#include <sys/sysctl.h>

#include <miscfs/procfs/procfs.h>

#include "lib/cpu.h"
#include "lib/malloc.h"
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

#pragma mark -
#pragma mark Linux-emulation functions

/*
 * Linux-compatible /proc/cpuinfo
 */
int
procfs_docpuinfo(__unused pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    vm_offset_t pageno, uva;
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
    uint32_t max_cpus = processor_count;

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
     * Here we can utilize the i386_cpu_info structure in i386/cpuid.h
     * to get the information we need. The cpuid_info() function sets up
     * the i386_cpu_info structure and returns a pointer to the structure.
     */
    char *vendor_id = cpuid_info()->cpuid_vendor;
    uint8_t cpu_family = cpuid_info()->cpuid_family;
    uint8_t model = cpuid_info()->cpuid_model + (cpuid_info()->cpuid_extmodel << 4);
    char *model_name = cpuid_info()->cpuid_brand_string;
    int microcode = (int)get_microcode_version();
    uint32_t cache_size = cpuid_info()->cpuid_cache_size;
    uint8_t stepping = cpuid_info()->cpuid_stepping;
    uint32_t cpu_cores = cpuid_info()->core_count;
    uint32_t cpuid_level = cpu_cores;
    uint32_t tlb_size = cpuid_info()->cache_linesize * 40;
    uint32_t clflush_size = cpuid_info()->cache_linesize;
    uint32_t cache_alignment = clflush_size;
    uint32_t addr_bits_phys = cpuid_info()->cpuid_address_bits_physical;
    uint32_t addr_bits_virt = cpuid_info()->cpuid_address_bits_virtual;

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
     * Allocate memory.
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

            /*
             * Reset the max_cpus variable at the end of each loop.
             * An unknown bug is causing the variable to reduce the
             * count down to 23 unless we do this.
             */
            if (max_cpus != processor_count) {
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
    free(buffer, M_TEMP);

    return 0;
}

/*
 * Linux-compatible /proc/loadavg
 */
int
procfs_doloadavg(__unused pfsnode_t *pnp, uio_t uio, __unused vfs_context_t ctx)
{
    int error = 0;
    int len = 0, xlen = 0;
    vm_offset_t off = uio_offset(uio);
    vm_offset_t pgno = trunc_page(off);
    off_t pgoff = (off - pgno);

    int lastpid = 30000;

    char *buf = malloc(LBFSZ, M_TEMP, M_WAITOK);

    unsigned int nrun = *(unsigned int *)avenrun;
    struct loadavg *avg = &averunnable;
    int i;

    avg->fscale = FSCALE;

    for (i = 0; i < 3; i++) {
        avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
            nrun * avg->fscale * (avg->fscale - cexp[i])) >> FSHIFT;
    }

    len = snprintf(buf, LBFSZ,
        "%d.%02d %d.%02d %d.%02d %d/%d %d\n",
        (int)(avg->ldavg[0] /       avg->fscale),
        (int)(avg->ldavg[0] * 100 / avg->fscale % 100),
        (int)(avg->ldavg[1] /       avg->fscale),
        (int)(avg->ldavg[1] * 100 / avg->fscale % 100),
        (int)(avg->ldavg[2] /       avg->fscale),
        (int)(avg->ldavg[2] * 100 / avg->fscale % 100),
        1,              /* number of running tasks */
        nprocs,         /* number of tasks */
        lastpid         /* the last pid */
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
    len = snprintf(buf, LBFSZ, "%s\n",
                                version);

    xlen = (len - pgoff);
    error = uiomove((const char *)buf, xlen, uio);

    free(buf, M_TEMP);

    return error;
}
