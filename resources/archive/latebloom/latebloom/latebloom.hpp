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
#ifndef LATEBLOOM_HPP
#define LATEBLOOM_HPP

class AAA_LoadEarly_latebloom : public IOService
{
   OSDeclareDefaultStructors(AAA_LoadEarly_latebloom)

public:
   // IOService overrides
   virtual bool start(IOService *provider) override;
   // 8sep21 v0.22 - dummy open() routine for /dev/latebloom pseudo-device
   static int LatebloomOpen(dev_t dev, int flags, int devetype, struct proc *p);

protected:

private:
};

//
// 8sep21 v0.22 - the function vectors for the /dev/latebloom pseudo-device
//
extern "C" struct cdevsw devsw; // avoid C++ name mangling
struct cdevsw devsw =
{
   .d_open  = AAA_LoadEarly_latebloom::LatebloomOpen,
};

#endif   // LATEBLOOM_HPP

