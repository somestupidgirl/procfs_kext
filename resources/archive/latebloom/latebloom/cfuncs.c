////////////////////////////////////////////////////////////////////////////////
//
// cfuncs.c
//
// C functions for latebloom
//
////////////////////////////////////////////////////////////////////////////////
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

#include <libkern/libkern.h>
#include <libkern/OSKextLib.h>
#include <kern/debug.h>
#include <IOKit/IOTypes.h>
#include <sys/conf.h>                  // 8sep21 v0.22 (for cdevsw_add())
#include <miscfs/devfs/devfs.h>        // 8sep21 v0.22 (for devfs_make_node())
// #include <IOKit/pwr_mgt/IOPMLib.h>  // For some strange reason, this won't ever #include properly
extern void IOSleep(unsigned int);     // Manually prototype IOSleep() here, since IOPMLib.h is problematic

#include "klookup.h"                   // Our kernel symbol lookup definitions

////////////////////////////////////////////////////////////////////////////////
//
// Version history:
// v0.17    Iniital public release of binary kext
// v0.18    (Limited test release)
// v0.19    Added support for Monterey beta 3
// v0.20    Added "lbloom=" condensed boot-args
// v0.21    Added "Phase1/Phase2" distinction (PCIe bus 0/1+)
//          Added "lb_delay2=", "lb_range2=", and additional "lbloom=" values
//          (Initial public release of source code)
// v0.22    Added creation of /dev/latebloom pseudo-device when hook is
//          successfully placed (allows confirmation that latebloom worked).
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// Many unsupported Macs, both genuine and Hackintosh, have difficulty booting
// MacOS Big Sur 11.3 and later (including all versions of Monterey to date).
// The symptom is that at a usually-consistent point in the boot process, the
// boot simply hangs.  To date, no one has definitively determined the exact
// cause of these hangs, despite many theories being presented.  Through trial
// and error experimentation, I have discovered that introducing a delay during
// enumeration of the PCIe buses seems to help mitigate the problem, without
// really shedding any light on the underlying cause.  This kext, <latebloom>,
// dynamically patches the MacOS kernel (actually, the IOPCIFamily.kext code)
// to introduce a delay into the PCIe bus enumeration loop.  Because the true
// problem may be the result of a race condition, latebloom supports random
// delays within a specified range, which may help avoid deadlock conditions.
//
// This kext's ability to inject its code is dependent upon finding a certain
// code pattern in the IOPCIFamily.kext code.  As new versions of MacOS are
// released, new patterns may need to be added to the code in order for
// latebloom to support those new versions.
//
// The latebloom kext is only active during PCIe bus enumeration, which
// appears to only happen once, during the early boot process.  After that,
// the kext is dormant, taking up a small amount of memory and never executing
// any code.  At present, no unloading mechanism is in place, so unloading
// the latebloom kext contains an element of risk - if latebloom is unloaded,
// the patched IOPCIFamily.kext code will point to memory that has been freed
// (and possibly reused), which would be a problem if the PCIe bus enumeration
// code ever executed again.  Fortunately, it appears that the code in question
// only executes during early boot, so while unloading latebloom produces an
// unclean result, the actual risk of a kernel panic or other disastrous
// outcome is extremely small.  If it ever becomes desirable or necessary to
// unload the latebloom kext, some cleanup code will need to be added.
//
// The code in this file contains various known inefficiencies.  It began its
// existence as a quick and dirty hack, and those origins still show.  Because
// it only executes once, and its purpose is to add imprecise delays, I have
// never found it worthwhile to clean up the code and optimize it.
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
// Macros/Constants
//
////////////////////////////////////////////////////////////////////////////////

#define MILLISECONDS_PER_SECOND  1000
#define LB_DEBUGMSG_PREFIX       "_____[ !!! *** latebloom *** !!! ]: " // all debug messages use this prefix
#define GET_SYMBOL(a,v) if ((v = SymbolLookup(a)) == NULL) { printf(LB_DEBUGMSG_PREFIX "failed to locate '%s', aborting\n", a); IOSleep(5 * MILLISECONDS_PER_SECOND); return; }
#define HOOK_WINDOW_SIZE         3144  // Maximum # bytes to search for hook placement
#define MAX_ARG_DIGITS           4     // Maximum number of digits in an boot-arg (xxx=NNNN)
#define DEFAULT_SLEEP            60    // Default sleep (milliseconds) if "latebloom=" is not specified
// 8sep21 v0.22 - for creating /dev/latebloom
#define STARTING_DEVSW_SLOT      -24   // per bsd/kern/bsd_stubs.c, -24 is a safe starting point (not -1)

////////////////////////////////////////////////////////////////////////////////
//
// Variables/Constants
//
// (Wherever possible, we use static variables to avoid possible collisions with
// the kernel, and avoid polluting the global namespace with clutter.  There's
// plenty of that already with all the bloated Apple code.)
//
////////////////////////////////////////////////////////////////////////////////

//
// Patterns for original hook (in the middle of the loop).
// These patterns need to be at least 14 bytes long, and end on an
// instruction boundary (not mid-instruction).
// Note that the choice of hook location relies on all instructions being
// neutral or register-relative, with no absolute or linker-supplied offsets.
// This way, we can simply append the original code to our hook code, and
// not worry about keeping track of which pattern was used at runtime.
//
static const unsigned char BytePattern113[] = {       // 11.3 to somewhere below 11.5b2
   0x48, 0xc7, 0x45, 0xd0, 0x00, 0x00, 0x00, 0x00,    // movq    $0x0, -0x30(%rbp)
   0x49, 0x8b, 0x06,                                  // movq    (%r14), %rax
   0x4c, 0x89, 0xf7                                   // movq    %r14, %rdi
   };
static const unsigned char BytePattern115b2[] = {     // 11.5b2 through 12.0b2
   0x48, 0xc7, 0x45, 0xd0, 0x00, 0x00, 0x00, 0x00,    // movq    $0x0, -0x30(%rbp)
   0x49, 0x8b, 0x07,                                  // movq    (%r15), %rax
   0x4c, 0x89, 0xff                                   // movq    %r15, %rdi
   };
// v0.19 - added support for Monterey Beta 3 (+?)
static const unsigned char BytePattern12b3[] = {      // 12.0b3+
   0x48, 0xc7, 0x45, 0xc8, 0x00, 0x00, 0x00, 0x00,    // movq    $0x0, -0x38(%rbp)
   0x49, 0x8b, 0x07,                                  // movq    (%r15), %rax
   0x4c, 0x89, 0xff                                   // movq    %r15, %rdi
   };
// (Patterns for alternate (top of loop) hook removed in v0.21)
// (All code related to alternate hook also removed in v0.21)

static struct
{
   const unsigned char *Pattern;
   const unsigned long size;
} BytePatterns[] =
{
   { BytePattern113,    sizeof(BytePattern113)     },
   { BytePattern115b2,  sizeof(BytePattern115b2)   },
   { BytePattern12b3,   sizeof(BytePattern12b3)    },
   { NULL,              0                          }, // Mark the end of the list
};

// Per-loop debug message (format string for printf())
static const char          HookMessage[] = LB_DEBUGMSG_PREFIX "PCI LOOP # %2ld %s delay %4ld ms (%08lx) *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_\n";
// Variations for internal/external PCI buses (the displayed names are somewhat arbitrary)
static const char          HookMessagePhase1[] = "ONBOARD";
static const char          HookMessagePhase2[] = "EXTERNAL";
// 8sep21 v0.22 - the name of our pseudo-device (in /dev/)
static const char          lbDeviceName[] = "latebloom";

static char                *BootArgs;                 // Our pointer to boot-args
static unsigned long long  lb_jump_address = 0;       // Address of the code we're hooking
static unsigned long       WhichPattern = 0;          // Which BytePattern is in use
static unsigned long       lb_PCI_counter = 0;        // IOPCIBridge::probeBus hook loop counter (for display only)
static unsigned long long  ProbeAddress = 0;          // Address of IOPCIBridge::probeBus
static unsigned long       SleepValue = 0;            // How long each loop should sleep (milliseconds)
static long                lb_DebugLevel = 0;         // Non-zero means display additional debug info
static long                lb_RandRange = 0;          // Range of random variations (+/-)
//
// It appears that the PCI probe loop first runs through PCI bus 0 (motherboard devices) using
// a single thread, then it goes multithreaded for the remaining buses/devices (PCIe cards and
// their children, if any, as well as the Ethernet and FireWire adapters).  Here we allow for
// two sleep/range values, one for bus 0 ("Phase 1"), the other for all other buses ("Phase 2").
// It may be that a longer delay on bus 0 will get the system booted, allowing a near-zero
// delay on the remaining buses.  Alternatively, it might be better to have a longer delay on
// the other buses, especially if there's an NVMe adapter in play.  The only way to find out is
// to experiment.
// Arguments are specified either separately:
//    lb_delay2=NNNN
//    lb_range2=NNNN
// or as part of the lbloom= argument:
//    lbloom=delay1,range1,debug,delay2,range2
// NOTE: In both cases, if delay2 is specified, that value is used (i.e. 0 means use delay of 0).
// The only way that delay2/range2 are NOT used (i.e. delay1/range1 are used for all PCI devices) is
// if delay2 is not specified - meaning there's no lb_delay2= boot-arg, or lbloom= does not contain
// anything past the debug parameter (no trailing comma after the debug parameter, if present).
//
static char                *CurrentThread = 0;        // Address of current thread
static long                lb_AltSleepValue = -1;     // "Phase 2" (EXTERNAL) sleep value. "-1" means "No P2 sleep specified" (this allows for 0)
static long                lb_AltRandRange = -1;      // "Phase 2" (EXTERNAL) random range - same default as lb_RandRange (no variation)
//
// 8sep21 v0.22 - we now create a dummy device (/dev/latebloom) if the hook is
// set successfully.  Below are the data elements we use for creating the
// /dev/latebloom pseudo-device.  At present, we don't actually use the device,
// so saving the device information here is unnecessary;  however, if we later
// decide to use the device to pass data back and forth, these will be useful.
//
extern struct cdevsw devsw;                           // Character device function vector table (see latebloom.hpp)
dev_t  fBaseDev;                                      // Our base device
int    MajorDev;                                      // Major device number
void   *fDeviceNode;                                  // Character device devfs node


////////////////////////////////////////////////////////////////////////////////
//
// Code
//
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////
//
// The hook code (outside of any C routine)
//
///////////////////////////////////////////////
asm (
   // The "_latebloom_fake: / callq ... / _latebloom_hook:" lines below allow us to calculate
   // the absolute address of IOPCIBridge::probeBus, since its symbol->address mapping
   // may not be readily available to us.
   // Modifying those three lines will break the kext.  Just leave them alone.
   "_latebloom_fake:                         \n"
   "  callq    __ZN11IOPCIBridge8probeBusEP9IOServiceh   \n"   // IOPCIBridge::probeBus(IOService *provider, UInt8 busNum), with C++ mangling
   "_latebloom_hook:                         \n"
   "  pushq    %rdi                          \n"   // Save all the registers.  We could probably prune this list a little bit,
   "  pushq    %rsi                          \n"   // but since we're *trying* to introduce delays, a few extra clock
   "  pushq    %rcx                          \n"   // cycles isn't going to hurt anything, and it gives us the freedom to
   "  pushq    %rbx                          \n"   // use whatever registers we might need inside the hook code.
   "  pushq    %rdx                          \n"
   "  pushq    %rax                          \n"
   "  pushq    %r15                          \n"
   "  pushq    %r14                          \n"
   "  pushq    %r13                          \n"
   "  pushq    %r12                          \n"
   "  pushq    %r11                          \n"
   "  pushq    %r10                          \n"
   "  pushq    %r9                           \n"
   "  pushq    %r8                           \n"
   //
   // See if we're in Phase 1 or Phase 2.
   // Phase 1 handles almost all of the onboard PCI devices.  It runs single-threaded,
   // meaning that current_thread() doesn't change from loop to loop.
   // Phase 2 handles all external PCI devices, as well as the Ethernet controllers and
   // FireWire controller.  It runs multi-threaded, so each of those threads will
   // have a different current_thread() value.
   // The first time through, our copy of CurrentThread is 0, so we just save current_thread()
   // to it.  From there, every iteration compares its current_thread() to CurrentThread.  The
   // first 55ish loops should match;  that constitutes Phase 1, and the delay/range are
   // specified by the original SleepValue/lb_RandRange.
   // If current_thread() doesn't match CurrentThread, we're in Phase 2, and we start using
   // lb_AltSleepValue/lb_AltRandRange (which might match SleepValue/lb_RandRange, or they
   // might be 0 - in the zero case, we just exit the loop instead of calling IOSleep(0).)
   //
   // (Note: We *could* do a symbol lookup on _current_thread() and call it each time through
   // the loop, but for simplicity, we just directly access it at %gs:0x10.  The obvious
   // downside to this is that the direct offset could change in some future MacOS version,
   // breaking latebloom in the process.  The upside to this is that we can load/compare
   // %gs:0x10 to any register we want, not having to dance around %rax (the return register
   // from _current_thread()).  Since the _current_thread() offset seems to be relatively
   // stable, we'll take our chances with it.)
   //
   // See if we've set CurrentThread yet
   "  movq     _CurrentThread(%rip),%rax     \n"
   "  testq    %rax,%rax                     \n"
   "  jnz      CTloaded                      \n"
   "  movq     %gs:0x10,%rax                 \n"
   "  movq     %rax,_CurrentThread(%rip)     \n"
   "CTloaded:                                \n"
   // At this point, %rax contains our copy of CurrentThread.
   "  cmpq     %gs:0x10,%rax                 \n"   // Have we gone multithreaded (entered Phase 2) yet?
   "  jz       LB_Phase1                     \n"   // current_thread() == CurrentThread, so we're in Phase 1
   // Phase 2 (external buses)
   //
   // 8sep21 v0.22 - once we're in Phase 2, try to create the /dev/latebloom node.
   // Logically, we'd do this when the hook is placed.  However, at that point in the
   // boot process, <devfs> has not yet been initialized, and calls to devfs_make_node()
   // always fail.  Once MacOS is multi-threaded (Phase 2), <devfs> will get set up,
   // and (hopefully) we'll succeed in creating /dev/latebloom during one of the
   // "external" (Phase 2) PCI bus probes.  Note that it's possible for the PCI bus
   // probes to finish quickly, or for <devfs> to initialize slowly, creating the
   // possibility that /dev/latebloom will not be created.  There's really not much we
   // can do about that, without jumping through a lot more hoops.
   //
   "  cmpq     $0,_fDeviceNode(%rip)         \n"   // if node is non-NULL, stop trying to create it
   "  jnz      LB_DeviceNodeMade             \n"
   "  xorl     %eax,%eax                     \n"
   "  movl     _fBaseDev(%rip),%edi          \n"   // arg1: Device ID
   "  movl     %eax,%esi                     \n"   // arg2: DEVFS_CHAR (::= 0)
   "  movl     %eax,%edx                     \n"   // arg3: UID_ROOT (::= 0)
   "  movl     %eax,%ecx                     \n"   // arg4: GID_WHEEL (::= 0)
   "  movl     $0x100,%r8d                   \n"   // arg5: Permissions (::= 0400)
   "  leaq     _lbDeviceName(%rip),%r9       \n"   // arg6: DeviceName (::= "latebloom")
   "  callq    _devfs_make_node              \n"   // devfs_make_node(fBaseDev, DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0400, "latebloom")
   "  movq     %rax,_fDeviceNode(%rip)       \n"   // save the result (NULL if it failed)
   "LB_DeviceNodeMade:                       \n"
   // end 8sep21 v0.22
   "  movl     _lb_AltSleepValue(%rip),%edi  \n"   // Calculate Phase 2 sleep value
   "  testl    %edi,%edi                     \n"
   "  jz       NoDebugOutput                 \n"   // if lb_AltSleepValue == 0, do nothing in Phase 2
   // If "lb_range2=" was not set, just use lb_AltSleepValue
   "  cmpl     $0,_lb_AltRandRange(%rip)     \n"
   "  jz       LB_DoSleep                    \n"
   //
   // We don't need strong randomization here, no particular distribution, just something
   // that's reasonably unpredictable.  We can do that cheaply using the RDTSC instruction.
   // RDTSC reads the TimeStamp Counter, which is a count of clock cycles since the CPU was
   // last reset.  For our purposes, it's more than random enough.  All we need to do is
   // constrain the values to our needs.
   // RDTSC reads the TSC into EDX:EAX.  For our purposes here, we only care about the low
   // 32 bits of that result (the most rapidly-changing part), which is in EAX.
   //
   "  rdtsc                                  \n"   // Result is in EDX:EAX
   "  xorl     %edx,%edx                     \n"   // We only care about the low 32 bits
   "  divl     _lb_AltRandRange(%rip)        \n"   // Result: EDX contains (random# % lb_AltRandRange)
   "  jmp      CTcontinue                    \n"   // Jump back to the common Phase1/Phase2 code
   // Calculate Phase 1 (Onboard bus) sleep value
   "LB_Phase1:                               \n"
   "  movl     _SleepValue(%rip),%edi        \n"   // Calculate the Phase 1 sleep value
   // If "lb_range=" was set, choose a random interval within the range
   "  cmpl     $0,_lb_RandRange(%rip)        \n"
   "  jz       LB_DoSleep                    \n"   // If effective range was 0, just sleep
   "  rdtsc                                  \n"   // Result is in EDX:EAX
   "  xorl     %edx,%edx                     \n"   // We only care about the low 32 bits
   "  divl     _lb_RandRange(%rip)           \n"   // Result: EDX contains (random# % lb_RandRange)
   // Phase 1 and Phase 2 code converges here
   "CTcontinue:                              \n"
   "  movl     %edx,%ebx                     \n"   // Preserve our (random# % RandRange) value (RDTSC modifies EDX)
   // %ebx now contains (rand() % lb_RandRange)
   "  rdtsc                                  \n"   // EAX contains low 32 bits of pseudo-random number
   "  movl     %ebx,%edx                     \n"   // Create the negative version of our value in EDX
   "  negl     %edx                          \n"
   "  testl    $0x01,%eax                    \n"   // Randomly add or subtract the range-bound random offset
   "  cmovnel  %edx,%ebx                     \n"
   "  addl     %ebx,%edi                     \n"
   // Back to common code
   "LB_DoSleep:                              \n"
   "  pushq    %rdi                          \n"   // Save our sleep value for later display
   "  callq    _IOSleep                      \n"   // Take a nap
   //
   // Note that if the loop counter were mission-critical, we'd need to use a lock, or
   // call OSAddAtomic, or otherwise protect against races during Phase 2 (multithreaded).
   // Since this counter is only used for display purposes, we don't really care if it's
   // perfectly accurate - at least, we don't care enough to bother enforcing atomicity.
   // Similarly, lb_LastSleep would also require some attention if accuracy was essential.
   //
   "  incl     _lb_PCI_counter(%rip)         \n"   // Increment the loop counter
   "  testl    $1,_lb_DebugLevel(%rip)       \n"   // If debug is enabled, print the updated counter
   "  popq     %rcx                          \n"   // Argument 3: value of this loop's sleep (align stack before possible jump)
   "  jz       NoDebugOutput                 \n"
   // The SysV ABI passes the first six arguments in RDI, RSI, RDX, RCX, R8, and R9.
   "  leaq     _HookMessage(%rip),%rdi       \n"   // Argument 0: the printf() format string
   "  leaq     _HookMessagePhase2(%rip),%rsi \n"   // Argument 2: either "ONBOARD" or "EXTERNAL"
   "  leaq     _HookMessagePhase1(%rip),%rdx \n"
   "  movq     %gs:0x10,%r8                  \n"   // Argument 5: current_thread()
   "  cmpq     _CurrentThread(%rip),%r8      \n"
   "  cmovne   %rsi,%rdx                     \n"   // If current_thread() != starting thread, use "EXTERNAL"
   "  andl     $0xffffffff,%r8d              \n"   // Mask off current_thread() (avoid redacted "<ptr>" output)
   "  movl     _lb_PCI_counter(%rip),%esi    \n"   // Argument 1: loop counter
   "  callq    _printf                       \n"
   "NoDebugOutput:                           \n"
   "  popq     %r8                           \n"   // We're done - pop all the registers we pushed
   "  popq     %r9                           \n"
   "  popq     %r10                          \n"
   "  popq     %r11                          \n"
   "  popq     %r12                          \n"
   "  popq     %r13                          \n"
   "  popq     %r14                          \n"
   "  popq     %r15                          \n"
   "  popq     %rax                          \n"
   "  popq     %rdx                          \n"
   "  popq     %rbx                          \n"
   "  popq     %rcx                          \n"
   "  popq     %rsi                          \n"
   "  popq     %rdi                          \n"
   // Reproduce the original code before jumping back in (kernel version-dependent)
   "_lb_hook_exit:                           \n"
   "  .byte 0x90, 0x90, 0x90, 0x90           \n"   // Here we need enough NOPs to exceed the size of our largest BytePattern.
   "  .byte 0x90, 0x90, 0x90, 0x90           \n"   // (We will copy the original code bytes into this NOP section
   "  .byte 0x90, 0x90, 0x90, 0x90           \n"   // so that the actions of the original code path are preserved.)
   "  .byte 0x90, 0x90, 0x90, 0x90           \n"   // NEED A MINIMUM OF 14 NOPS HERE;  more allows us flexibility
   "  .byte 0x90, 0x90, 0x90, 0x90           \n"   // (to be forgetful about counting) when adding new patterns.
   "  jmpq     *_lb_jump_address(%rip)       \n"   // Jump into the original code, just past our "jump to hook" patch
);


//
// Worker function for boot-args parsing
//
static long ExtractArgValue(char *StartPos)
{
   long  lbval = 0;
   int   j;

   // Scan for up to four decimal digits, using default value of 0.
   // Stop on first non-digit.
   for (j = 0; j < MAX_ARG_DIGITS && StartPos[j] >= '0' && StartPos[j] <= '9'; ++j)
   {
      lbval = (lbval * 10) + (StartPos[j] - '0');
   }
   return lbval;
}

/////////////////////////////////////////////////////////
//
// This is called when latebloom is initialized by the kernel.
//
/////////////////////////////////////////////////////////
void latebloom_start(void)
{
   int i, j;
   unsigned char *ptr;
   // Local symbols defined in the assembly language above (invisible to the C compiler without extern declarations):
   extern unsigned long long latebloom_hook; // The address of our hook code
   extern unsigned long long lb_hook_exit;   // The address of our hook exit code

   //
   // Before anything, see if we're running Big Sur or later.  If not, just bail.
   // (Don't bother with the minor version, the BytePattern mismatch will take care of it.)
   //
   if (version_major < BIGSUR_XNU_MAJOR_VERSION)   // Big Sur is 20, Monterey is 21
   {
      printf("\n\n\n\n" LB_DEBUGMSG_PREFIX "OS major version == %d, aborting (HOOK NOT PLACED).\n", version_major);
      IOSleep(4 * MILLISECONDS_PER_SECOND);  // Delay long enough to read the message
      return;
   }

   printf(LB_DEBUGMSG_PREFIX "Starting.\n");
   // Some variables are only actually used in the assembly code.  The compiler creates
   // assembly code, which the assembler then handles;  because of this, symbol definitions
   // and usage in the C source are visible to the inline assembly code, but definitions
   // and usage in the inline assembly code are not visible in the C portion, so the
   // compiler/assembler gets confused.
   // If the following bogus assignments aren't present, the compiler drops the "unused" variables,
   // which then causes "undefined symbol" errors at load time.
   // It's nice that they allow inline assembly;  I just wish it was a more robust implementation.
   CurrentThread = 0;
   ptr = (unsigned char *)HookMessage;
   ptr = (unsigned char *)HookMessagePhase1;
   ptr = (unsigned char *)HookMessagePhase2;
   ptr = (unsigned char *)lbDeviceName;            // 8sep21 v0.22
   // End bogus assignments to keep the compiler happy

   if (SleepValue == 0)    // Either it's the first time through or the user set it to 0
   {
      // First, get the address of the _PE_boot_args() function.
      // _PE_boot_args() returns a pointer to the boot-args string.
      // We re-purpose the <BootArgs> variable here, using it first as a pointer
      // to the _PE_boot_args() function, then as a pointer to the boot-args
      // string itself.
      GET_SYMBOL("_PE_boot_args", BootArgs)        // Get the address of _PE_boot_args()

      // Get pointer to actual boot args by calling _PE_boot_args() and re-purposing the BootArgs variable
      asm (
         "movq    _BootArgs(%rip),%rax       \n"   // Get the address of the PE_boot_args() function
         "callq   *%rax                      \n"   // Call PE_boot_args()
         "movq    %rax,_BootArgs(%rip)       \n"   // Save the address of boot-args in the same variable
         );                                        // BootArgs now points to the boot-args string

      printf(LB_DEBUGMSG_PREFIX "boot-args = %s\n", BootArgs);

//
// These macros help clarify the code below.  We use macros instead of functions because they use some local
// variables (such as <arglen>, <ptr>, and <j>) that would be clumsy to pass back and forth, and subsequent code
// uses these same variables, so they'd have to be passed by reference, making things even uglier.  This part
// isn't extremely pretty, but it's effective, and efficiency isn't at a premium here;  we only parse boot-args
// once.
//
#define BOOTARG_MATCH(zstr) (!strncmp(&BootArgs[i], zstr, arglen = (sizeof(zstr) - 1)))
#define EXTRACT_LBLOOM_VALUE { ptr += j + 1; lbval = 0; for (j = 0; j < MAX_ARG_DIGITS && ptr[j] >= '0' && ptr[j] <= '9'; ++j) { lbval = (lbval * 10) + (ptr[j] - '0'); } }

      // This loop is inefficient, but we only do it once, and the data set is small, so...
      for (i = 0; BootArgs[i] != '\0'; ++i)
      {
         int arglen;

         if (BOOTARG_MATCH("latebloom="))
         {
            long lbval = -1;

            ptr = (unsigned char *)&BootArgs[i + arglen];
            for (j = 0; j < MAX_ARG_DIGITS && ptr[j] >= '0' && ptr[j] <= '9'; ++j)
            {
               if (lbval == -1)
               {
                  lbval = 0;
               }
               lbval = (lbval * 10) + (ptr[j] - '0');
            }
            if (lbval <= 0)
            {
               printf("\n\n" LB_DEBUGMSG_PREFIX "latebloom=0, NOT INSTALLING HOOK\n\n");
               IOSleep(4 * MILLISECONDS_PER_SECOND);  // delay long enough to read the message
               return;
            }
            SleepValue = lbval;
         }
         else if (BOOTARG_MATCH("lb_debug="))
         {
            lb_DebugLevel = ExtractArgValue(&BootArgs[i + arglen]);
            printf(LB_DEBUGMSG_PREFIX "lb_debug set to %ld\n", lb_DebugLevel);
         }
         else if (BOOTARG_MATCH("lb_range="))
         {
            lb_RandRange = ExtractArgValue(&BootArgs[i + arglen]);
            printf(LB_DEBUGMSG_PREFIX "lb_range set to %ld\n", lb_RandRange);
         }
         // v0.21 - added Phase 1/Phase 2 differentiation
         else if (BOOTARG_MATCH("lb_delay2="))
         {
            lb_AltSleepValue = ExtractArgValue(&BootArgs[i + arglen]);
            printf(LB_DEBUGMSG_PREFIX "lb_delay2 set to %ld\n", lb_AltSleepValue);
         }
         // v0.21 - added Phase 1/Phase 2 differentiation
         else if (BOOTARG_MATCH("lb_range2="))
         {
            lb_AltRandRange = ExtractArgValue(&BootArgs[i + arglen]);
            printf(LB_DEBUGMSG_PREFIX "lb_range2 set to %ld\n", lb_AltRandRange);
         }
         // v0.20 - added "lbloom=" condensed boot-arg
         else if (BOOTARG_MATCH("lbloom="))  // condensed latebloom parameters
         {
            // Format is: lbloom=delay,range,debug,delay2,range2
            // (SleepValue, lb_RandRange, lb_DebugLevel, lb_AltSleepValue, lb_AltRandRange)
            // Args can be omitted, e.g. "lbloom=90,,1" or "lbloom=110" or "lbloom=,,1" or "lbloom="90,20"
            // Omitted args are always treated as 0
            // Values > 4 digits cause all subsequent values to be 0 (e.g. "lbloom=12345,90,1" effectively becomes "1234,0,0")
            long lbval = 0;
            ptr = (unsigned char *)&BootArgs[i + arglen];
            SleepValue = 0;
            lb_RandRange = 0;
            lb_DebugLevel = 0;
            // First, get the delay value (if any)
            for (j = 0; j < MAX_ARG_DIGITS && ptr[j] >= '0' && ptr[j] <= '9'; ++j)
            {
               lbval = (lbval * 10) + (ptr[j] - '0');
            }
            if (lbval == 0)
            {
               printf("\n\n" LB_DEBUGMSG_PREFIX "lbloom=0, NOT INSTALLING HOOK\n\n");
               IOSleep(4 * MILLISECONDS_PER_SECOND);  // delay long enough to read the message
               return;
            }
            SleepValue = lbval;
            if (ptr[j] == ',')   // are there more arguments to follow?
            {
               // Next is lb_range
               EXTRACT_LBLOOM_VALUE
               lb_RandRange = lbval;
            }
            if (ptr[j] == ',')   // are there more arguments to follow?
            {
               // Next is lb_debug
               EXTRACT_LBLOOM_VALUE
               lb_DebugLevel = lbval;
            }
            if (ptr[j] == ',')   // are there more arguments to follow?
            {
               // Next is lb_delay2
               EXTRACT_LBLOOM_VALUE
               // if argument is omitted (not explicitly zero), use SleepValue instead
               lb_AltSleepValue = (j == 0) ? SleepValue : lbval;
            }
            if (ptr[j] == ',')   // are there more arguments to follow?
            {
               // Next is lb_range2
               EXTRACT_LBLOOM_VALUE
               lb_AltRandRange = lbval;
            }
            // Assuming they're all comma-separated, additional arguments can appear here
            // (just use the "if (ptr[j] == ',') { ... }" above as a template)
         }  // end else if (BOOTARG_MATCH("lbloom="))
      }  // end for (i = 0; BootArgs[i] != '\0'; ++i)
      //
      // If we get here, we're going to place the hook and do some delays.  The Phase 2 delay might
      // be 0 ms, but we're hooking anyway.
      //
      // Before we do anything else, deal with any defaults that need to be set.
      //
      if (SleepValue == 0)    // latebloom= value not specified, so use default
      {
         SleepValue = DEFAULT_SLEEP;
         printf(LB_DEBUGMSG_PREFIX "latebloom boot-arg not set, Phase 1 using %lu ms default.\n", SleepValue);
      }
      else
      {
         printf(LB_DEBUGMSG_PREFIX "based on boot-args, Phase 1 using delay of %lu ms.\n", SleepValue);
      }
      if (lb_AltSleepValue != -1)
      {
         printf(LB_DEBUGMSG_PREFIX "based on boot-args, Phase 2 using delay of %lu ms.\n", lb_AltSleepValue);
      }
      else
      {
         lb_AltSleepValue = SleepValue;
         printf(LB_DEBUGMSG_PREFIX "No Phase 2 delay specified, using Phase 1 delay of %lu ms.\n", lb_AltSleepValue);
      }
      if (lb_RandRange != 0)
      {
         if (lb_RandRange > SleepValue)
         {
            lb_RandRange = SleepValue;
            printf(LB_DEBUGMSG_PREFIX "lb_range larger than lb_sleep, truncating to %ld\n", lb_RandRange);
         }
         printf(LB_DEBUGMSG_PREFIX "Phase 1 delays will be random, between %lu and %lu ms.\n",
                SleepValue - lb_RandRange, SleepValue + lb_RandRange);
      }
      if (lb_AltRandRange != 0)
      {
         if (lb_AltRandRange == -1)
         {
            lb_AltRandRange = lb_RandRange;
         }
         if (lb_AltRandRange > lb_AltSleepValue)
         {
            lb_AltRandRange = lb_AltSleepValue;
            printf(LB_DEBUGMSG_PREFIX "lb_range2 larger than lb_delay2, truncating to %ld\n", lb_AltRandRange);
         }
         if (lb_AltRandRange != 0)
         {
            printf(LB_DEBUGMSG_PREFIX "Phase 2 delays will be random, between %lu and %lu ms.\n",
                   lb_AltSleepValue - lb_AltRandRange, lb_AltSleepValue + lb_AltRandRange);
         }
      }
   }  // end if (SleepValue == 0)

   //
   // Find our insertion point and place the hook
   //
   if (lb_jump_address == 0)  // We haven't yet calculated the hook address, so the hook is not yet set
   {
      printf(LB_DEBUGMSG_PREFIX "Start - First time through, trying to place hook...\n");
      // Find the symbol (which isn't part of the kernel symbol table)
      asm (
         "  pushq %rdi                             \n"   // Save the registers we'll use (don't confuse the compiler)
         "  pushq %rax                             \n"
         "  movl  _latebloom_fake+1(%rip),%eax     \n"   // Get the 32-bit offset of IOPCIBridge::probeBus from our fake call
         "  movsx %eax,%rax                        \n"   // Sign-extend to 64 bits
         "  leaq  _latebloom_hook(%rip),%rdi       \n"   // Get the 64-bit address of our hook code
         "  addq  %rdi,%rax                        \n"   // Adding the offset gets us the 64-bit address of IOPCIBridge::probeBus
         "  movq  %rax,_ProbeAddress(%rip)         \n"   // Save it
         "  popq  %rax                             \n"   // Restore the registers we used
         "  popq  %rdi                             \n"
         );
      //
      // Search IOPCIBridge::probeBus() for a byte pattern that we recognize.
      // (This could be made significantly more efficient, but we only do it once,
      // and the window is relatively small, so "simple but inefficient" is fine.)
      //
      ptr = (unsigned char *)ProbeAddress;
      for (i = 0; i < HOOK_WINDOW_SIZE && lb_jump_address == 0; ++i, ++ptr)
      {
         for (WhichPattern = 0; BytePatterns[WhichPattern].Pattern != NULL; ++WhichPattern)
         {
            if (!memcmp(ptr, BytePatterns[WhichPattern].Pattern, BytePatterns[WhichPattern].size))
            {
               lb_jump_address = (unsigned long long)ptr;
               break;
            }
         }
      }  // end for (i = 0; i < HOOK_WINDOW_SIZE; ++i)

      // Did we find a byte pattern that we can use?
      if (lb_jump_address == 0)
      {
         printf("\n\n" LB_DEBUGMSG_PREFIX "Hook byte pattern not found, HOOK NOT PLACED. ...---...\n\n");
         IOSleep(4 * MILLISECONDS_PER_SECOND);  // delay long enough to read the message
         return;
      }

      // We found a place to set our hook.
      asm (
         // Turn off interrupts, make codespace writable
         "  cli                                    \n"   // Interrupts off
         "  movq  %cr0,%rax                        \n"   // Get current CR0
         "  pushq %rax                             \n"   // Save original CR0 for later restoration
         "  pushq %rbx                             \n"   // Save RBX (don't confuse the compiler)
         "  movq  $0xfffffffffffeffff,%rbx         \n"   // Mask to enable write to codespace
         "  andq  %rbx,%rax                        \n"   // Mask CR0 to make codespace writable
         "  movq  %rax,%cr0                        \n"   // Update CR0 (make codespace writable)
         // Write the hook.
         // Note that we can't rely on the IOPCIFamily.kext being within +/- 2GB of our kext, so
         // we need to do a 64-bit long jump.  Since we don't control the registers at that point,
         // we can't load a register with our address and do an indirect jump without trashing the
         // register;  alternatively, pushing a register, loading it with an Imm64 address, then
         // jumping indirectly through it takes a minimum of 15 bytes.
         // Instead, we can leverage the RIP-relative mechanism and jump indirectly through a
         // 64-bit memory location;  by using an offset of 0 and immediately following that with
         // our 64-bit target address, we can always jump anywhere in exactly 14 bytes - and not
         // modify any registers (other than RIP) in the process.
         "  movq  _lb_jump_address(%rip),%rbx      \n"   // Where the hook is going to be placed
         "  movl  $0x25ff,%eax                     \n"   // <ff 25 00 00 00 00> is "jmp *(%rip)"
         "  movq  %rax,(%rbx)                      \n"   // Write eight bytes, although we only need the first six
         "  leaq  _latebloom_hook(%rip),%rax       \n"   // 64-bit address of our hook code
         "  addq  $6,%rbx                          \n"   // Point past the "jmp *(%rip)"
         "  movq  %rax,(%rbx)                      \n"   // Save our 64-bit target address
         "  addq  $8,%rbx                          \n"   // Point past the long jump
         "  movq  %rbx,_lb_jump_address(%rip)      \n"   // Save this address as the return point from our hook
         "  popq  %rbx                             \n"   // Restore RBX
         );
      // Copy the selected byte pattern to the end of our hook code (overwriting the NOPs we put there for this purpose)
      ptr = (unsigned char *)&lb_hook_exit;
      memcpy(ptr, BytePatterns[WhichPattern].Pattern, BytePatterns[WhichPattern].size);
      // Make codespace read-only again, turn interrupts back on
      asm (
         "  popq  %rax                             \n"   // Retrieve original CR0
         "  movq  %rax,%cr0                        \n"   // Restore original CR0 (make codespace read-only again)
         "  sti                                    \n"   // Interrupts back on
         );
      // Verbosely log our success
      printf(LB_DEBUGMSG_PREFIX "Hook placed successfully.  Count = %d :: %d,%d,%d,%d,%d\n", (int)lb_PCI_counter, (int)SleepValue, (int)lb_RandRange, (int)lb_DebugLevel, (int)lb_AltSleepValue, (int)lb_AltRandRange);
      //
      // 8sep21 v0.22 - if we successfully set the hook, also create /dev/latebloom as
      // an indicator.
      //
      // Add the device to the system cdevsw table
      MajorDev = cdevsw_add(STARTING_DEVSW_SLOT, &devsw);
      if (MajorDev >= 0)   // Successfully added cdevsw?
      {
         // Build the device ID using our major # and a minor # of 0
         fBaseDev = makedev(MajorDev, 0);
         // Create /dev/latebloom visibly in the filesystem
         // NOTE: While this is the logical place to create /dev/latebloom, the
         // fact is that at this point in the boot process, <devfs> has not yet
         // been initialized, so the following call to devfs_make_node() will
         // surely fail.  We call it here to initialize fDeviceNode (to NULL if
         // it fails, or to a device handle if, against all odds, it succeeds),
         // and let the hook code try to create the node once MacOS has gone
         // multi-threaded.
         fDeviceNode = devfs_make_node(fBaseDev,               // Our device ID
                                       DEVFS_CHAR,             // Device type
                                       UID_ROOT,               // Owner UID
                                       GID_WHEEL,              // Owner GID
                                       0400,                   // Permissions
                                       (char *)"latebloom");   // Device name
      }
   }  // end if (lb_jump_address == 0)

   // All done
   return;
}

