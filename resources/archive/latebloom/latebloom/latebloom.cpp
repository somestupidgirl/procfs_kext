//
//  latebloom.cpp
//
//  C++ wrapper for latebloom functions.
//  Since IOKit expects a C++ class, it's simplest to just wrap
//  our C and assembly code with a thin layer of C++.
//
//  Created by Syncretic on 29jun21.
//
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
#include <sys/cdefs.h>
__BEGIN_DECLS
//
// Note: on my system, XCode complains about empty structures in some of the IOKit headers.
// Here we disable warnings while we include system headers (we'll re-enable them right after).
// This should be safe, since system headers are hypothetically warning-free...
//
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"

#include <mach/mach_types.h>
#include <libkern/libkern.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/dkstat.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <miscfs/devfs/devfs.h>
#include <i386/proc_reg.h>
#include <kern/thread.h>
#include <libkern/version.h>
#include <IOKit/assert.h>
#include <IOKit/system.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOService.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOTypes.h>

// Re-enable warnings now that we're done including system headers
#pragma clang diagnostic pop

#include "latebloom.hpp"
__END_DECLS

// This required macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires.
OSDefineMetaClassAndStructors(AAA_LoadEarly_latebloom, IOService)

// Define the driver's superclass.
#define super IOService

/////////////////////////////////////////////////////////////////
//
// Because we specified MODULE_START as latebloom_start() in the project's target settings, that routine
// will be called at load time.  IOKit's call to latebloom::start() won't happen until later.  Here, we
// create a dummy IOKit start() routine, just to have a handy place to add some code if at some point we
// decide that doing something at IOKit startup would be useful.
//
/////////////////////////////////////////////////////////////////
bool AAA_LoadEarly_latebloom::start(IOService *provider)
{
   bool result = super::start(provider);

   return result; // true if successful, false if not
}

/////////////////////////////////////////////////////////////////
//
// The reality is that at unload time, neither ->stop() nor
// ->free() seem to get called reliably.  Because of that,
// latebloom can't really be unloaded.  We can't do anything
// about that here, anyway.  If ->stop() was called, we could
// undo our hook and allow latebloom to be unloaded.  However,
// since latebloom is quiescent by the time the desktop appears,
// and it uses very little memory, there's not much point in
// unloading it anyway.  Because it's seemingly never called,
// we don't bother implementing an IOKit stop() method.
//
// NOTE that if someone does try to unload latebloom, and the
// kernel obliges, IOPCIBridge::probeBus() will still contain a
// jump into memory that has now been freed;  in the unlikely
// event that the PCI bus was somehow probed again, this would
// result in a kernel panic (or, conceivably, something worse).
//
/////////////////////////////////////////////////////////////////

//
// 8sep21 v0.22 - we now create /dev/latebloom if the hook gets set successfully.
// However, at present, we don't actually want to handle any I/O with the
// device, we just need a convenient signal that latebloom worked.  Here we
// implement the device::open() routine to simply fail, regardless of the type
// of open that's requested.  If, in the future, we want to pass data in and
// out of the device, we'll need to implement at least the open/read/write/close
// methods, as well as possibly ioctl/stop/reset/getc/putc/type (or others).
//
int AAA_LoadEarly_latebloom::LatebloomOpen(dev_t dev, int flags, int devetype, struct proc *p)
{
   return KERN_FAILURE;    // for now, always fail to open the device.
}
