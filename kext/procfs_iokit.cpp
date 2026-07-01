/*
 * Copyright (c) 2022-2026 Sunneva N. Mariu
 *
 * procfs_iokit.cpp
 *
 * IOKit block-device enumeration for the partitions node. Linux's
 * /proc/partitions lists every block device (whole disks and partitions,
 * mounted or not); on macOS that information lives in the IORegistry, reachable
 * only through the C++ IOKit runtime (the C IOKit KPI exposes no registry
 * matching). This is the kext's one C++ translation unit; it exposes a single
 * C-linkage entry point that the C partitions node calls.
 *
 * We match the "IOMedia" class by name and read properties off the base
 * IORegistryEntry, so no dependency on IOStorageFamily's IOMedia C++ class (and
 * its metaclass) is needed - only the base IOKit and libkern KPIs, both already
 * declared in Info.plist.
 */
#include <IOKit/IOService.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSIterator.h>
#include <libkern/libkern.h>
#include <sys/errno.h>

#include <fs/procfs/procfs_iokit.h>

/* IORegistry property keys (stable IOKit strings; hardcoded to avoid pulling in
 * IOStorageFamily / IOBSD headers). */
#define PROCFS_IOMEDIA_CLASS    "IOMedia"
#define PROCFS_KEY_BSD_NAME     "BSD Name"
#define PROCFS_KEY_BSD_MAJOR    "BSD Major"
#define PROCFS_KEY_BSD_MINOR    "BSD Minor"
#define PROCFS_KEY_SIZE         "Size"

extern "C" int
procfs_iokit_get_partitions(struct procfs_partition *out, int max, int *count)
{
    if (out == nullptr || count == nullptr || max <= 0) {
        return EINVAL;
    }
    *count = 0;

    OSDictionary *match = IOService::serviceMatching(PROCFS_IOMEDIA_CLASS);
    if (match == nullptr) {
        return ENOMEM;
    }

    /* getMatchingServices consumes no reference to `match`; release it after. */
    OSIterator *it = IOService::getMatchingServices(match);
    match->release();
    if (it == nullptr) {
        return EIO;
    }

    int n = 0;
    OSObject *obj;
    while (n < max && (obj = it->getNextObject()) != nullptr) {
        IORegistryEntry *entry = OSDynamicCast(IORegistryEntry, obj);
        if (entry == nullptr) {
            continue;
        }

        OSString *name = OSDynamicCast(OSString, entry->getProperty(PROCFS_KEY_BSD_NAME));
        if (name == nullptr) {
            continue;                       /* no BSD device - skip */
        }

        OSNumber *major = OSDynamicCast(OSNumber, entry->getProperty(PROCFS_KEY_BSD_MAJOR));
        OSNumber *minor = OSDynamicCast(OSNumber, entry->getProperty(PROCFS_KEY_BSD_MINOR));
        OSNumber *size  = OSDynamicCast(OSNumber, entry->getProperty(PROCFS_KEY_SIZE));

        struct procfs_partition *p = &out[n];
        strlcpy(p->name, name->getCStringNoCopy(), sizeof(p->name));
        p->major = major != nullptr ? major->unsigned32BitValue() : 0;
        p->minor = minor != nullptr ? minor->unsigned32BitValue() : 0;
        p->size  = size  != nullptr ? size->unsigned64BitValue()  : 0;
        n++;
    }
    it->release();

    *count = n;
    return 0;
}
