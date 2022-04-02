/*
 * Copyright (c) 2022 Sunneva Jonsdottir
 *
 * procfs_cpu.c
 *
 * Functions to provide CPU information and statistics.
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

#include <miscfs/procfs/procfs.h>

#include <cpulib/cpu.h>
#include <symlib/symbols.h>

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
    uint8_t model = _cpuid_info()->cpuid_model + (_cpuid_info()->cpuid_extmodel << 4);
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
    char *pm = "";
    char *x86_bugs = "";
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

    return 0;
}
