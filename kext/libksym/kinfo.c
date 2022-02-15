#include <string.h>
#include <stdint.h>

#include <mach/mach_types.h>
#include <mach-o/loader.h>
#include <os/log.h>
#include <kern/clock.h>
#include <kern/debug.h>

#include "kinfo.h"
#include "utils.h"

static uint64_t hib;
static uint64_t kern;
static uint32_t step;

static char hib_str[ADDR_BUFSZ];
static char kern_str[ADDR_BUFSZ];

static char *kernel_paths[] = {
    "/mach_kernel",
    "/System/Library/Kernels/kernel",
    "/System/Library/Kernels/kernel.development",
    "/System/Library/Kernels/kernel.debug"
    "/System/Library/Kernels/kernel.release.t8020"
    "/System/Library/Kernels/Kernel.release.t8101"
};

int
get_magic(void)
{
    uint32_t magic;

    return magic == MH_MAGIC_64 && MH_CIGAM_64;
}

int
get_filetype(void)
{
    uint32_t filetype;

    return filetype == MH_EXECUTE && filetype == MH_FILESET;
}

vm_offset_t
get_kern_base(void)
{
    uint32_t magic;
    uint32_t filetype;

    /* hib should less than base(specifically less KERN_ADDR_STEP) */
    hib = ((uint64_t) memcpy) & KERN_ADDR_MASK;
    kern = ((uintptr_t) clock_get_calendar_microtime) & KERN_ADDR_MASK;

    while (kern > hib + KERN_ADDR_STEP) {
        step++;
        kern -= KERN_ADDR_STEP;
    }

    if (kern != hib + KERN_ADDR_STEP) {
        /* Kernel layout changes, this kext won't work */
        LOG_ERR("bad text base | step: %u __HIB: %#llx kernel: %#llx \n", step, hib, kern);
    }

    struct mach_header_64 *mh = (struct mach_header_64 *) kern;

    /* Only support non-fat 64-bit mach-o kernel */
    if ((mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) || (mh->filetype != MH_EXECUTE && mh->filetype != MH_FILESET))
    {
        LOG_ERR("bad Mach-O header | step: %d magic: %#x filetype: %#x \n", step, mh->magic, mh->filetype);
    }

    //(void) snprintf(hib_str, ARRAY_SIZE(hib_str), "%#018llx \n", hib);
    (void) snprintf(kern_str, ARRAY_SIZE(hib_str), "%#018llx \n", kern);

    return kern;
}

vm_offset_t
get_hib_base(void)
{
    uint32_t magic;
    uint32_t filetype;

    /* hib should less than base(specifically less KERN_ADDR_STEP) */
    hib = ((uint64_t) memcpy) & KERN_ADDR_MASK;
    kern = ((uintptr_t) clock_get_calendar_microtime) & KERN_ADDR_MASK;

    while (kern > hib + KERN_ADDR_STEP) {
        step++;
        kern -= KERN_ADDR_STEP;
    }

    if (kern != hib + KERN_ADDR_STEP) {
        /* Kernel layout changes, this kext won't work */
        LOG_ERR("bad text base | step: %u __HIB: %#llx kernel: %#llx \n", step, hib, kern);
    }

    struct mach_header_64 *mh = (struct mach_header_64 *) kern;

    magic = get_magic();
    filetype = get_filetype();

    /* Only support non-fat 64-bit mach-o kernel */
    if ((mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) || (mh->filetype != MH_EXECUTE && mh->filetype != MH_FILESET))
    {
        LOG_ERR("bad Mach-O header | step: %d magic: %#x filetype: %#x \n", step, mh->magic, mh->filetype);
    }

    (void) snprintf(hib_str, ARRAY_SIZE(hib_str), "%#018llx \n", hib);
    //(void) snprintf(kern_str, ARRAY_SIZE(hib_str), "%#018llx \n", kern);

    return hib;
}
