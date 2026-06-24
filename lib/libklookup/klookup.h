////////////////////////////////////////////////////////////////////////////////
// License: 0BSD                                                              //
//                                                                            //
// Copyright (C) 2021 by Syncretic                                            //
// Copyright (C) 2026 by Sunneva N. Mariu                                     //
//                                                                            //
// Permission to use, copy, modify, and/or distribute this software for any   //
// purpose with or without fee is hereby granted.                             //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES   //
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF           //
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR    //
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES     //
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      //
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR //
// IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifndef KLOOKUP_H
#define KLOOKUP_H

#include <mach/mach_types.h>
#include <mach-o/loader.h>
#include <sys/systm.h>
#include <sys/types.h>
/*
 * Lock struct definitions must be included before sys/sysctl.h.
 * On ARM64 the public SDK arm/locks.h only forward-declares these
 * structs, so we vendor the full definitions from XNU open source.
 * On x86_64 the SDK Kernel.framework already provides complete
 * definitions so no vendored header is needed.
 */
#if defined(__arm64__) || defined(__aarch64__)
#include <arm/locks.h>
#endif
#include <sys/sysctl.h>
#include <vm/vm_kern.h>
#include <libkern/version.h>

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * Resolve `count` symbol names to runtime addresses (out[i] = address or
     * NULL). Returns the number resolved. Reads the on-disk kernelcache once,
     * so batch everything you need in a single call.
     */
    int klookup_resolve(const char *const *names, void **out, int count);

    /* Single-symbol convenience wrapper around klookup_resolve(). */
    void *SymbolLookup(const char *symbol);

#ifdef __cplusplus
}
#endif

#define BIGSUR_XNU_MAJOR_VERSION    20    /* Darwin 20 = macOS Big Sur */

#endif /* KLOOKUP_H */