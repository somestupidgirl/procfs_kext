////////////////////////////////////////////////////////////////////////////////
// License: 0BSD                                                              //
//                                                                            //
// Copyright (C) 2021 by Syncretic                                            //
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
#include <vm/vm_kern.h>
#include <sys/sysctl.h>
#include <libkern/version.h>

#ifdef __cplusplus
extern "C" {
#endif

    void *SymbolLookup(const char *symbol);

#ifdef __cplusplus
}
#endif

#define BIGSUR_XNU_MAJOR_VERSION    20    // the major version # (version_major) of the Big Sur kernel (Monterey is 21)

#endif // KLOOKUP_H

