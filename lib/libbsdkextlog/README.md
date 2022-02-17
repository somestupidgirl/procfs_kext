Original: https://github.com/somestupidgirl/bsd_kext_log

## bsd\_kext\_log

`bsd_kext_log` is a macOS kernel extension used for experimental kext logging propagation from kernel into user space.

### Rationale

One can use [log(1)](x-man-page://1/log)/[syslog(1)](x-man-page://1/syslog) to capture log from kernel, just as [kextlogd](https://github.com/lynnlx/kextlogd) do.  Yet due to kext cache mechanism, new logging system [log(1)](x-man-page://1/log) may capture nothing while the kext was indeed issued a log via [printf](http://xr.anadoxin.org/source/xref/macos-10.13.6-highsierra/xnu-4570.71.2/osfmk/kern/printf.c#853) or [IOLog](http://xr.anadoxin.org/source/xref/macos-10.13.6-highsierra/xnu-4570.71.2/iokit/Kernel/IOLib.cpp#1152).

Thusly this kernel extension tries to fix this flaw by introducing a simple in-kernel logging mechanism, which it uses [kernel control socket](https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/NKEConceptual/control/control.html) to communicate with user space log-daemon client, and pushing messages to it(daemon).

So user space daemon can do bottom-half work like print the log, persist into disk, etc.

This logging technique inspired by Microsoft's [VFS for Git](https://github.com/Microsoft/VFSForGit)(ProjFS.Mac), yet they use [IOSharedDataQueue](http://xr.anadoxin.org/source/xref/macos-10.13.6-highsierra/xnu-4570.71.2/iokit/IOKit/IOSharedDataQueue.h#55) for messaging propagation, which is part of [`IOKit` environment](https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/Architecture/Architecture.html#//apple_ref/doc/uid/TP30000905-CH1g-TPXREF102), unavailable for BSD portion.

### Log interface

There're essentially five log levels:

* TRACE - (Log call path, massive logs, logging persistence is optional)

* DEBUG - (Used for debugging purpose, will print nothing in release build)

* INFO - (Usual info, should always persist into disk)

* WARNING - (Used for negligible errors)

* ERROR - (For significant errors should be take care of)

Logging function:

```c
void log_printf(uint32_t, const char *, ...) __printflike(2, 3);
```

which the first parameter indicates the log level, there're already candy wrappers for each levels:

```c
log_trace(fmt, ...);
log_debug(fmt, ...)
log_info(fmt, ...);
log_warning(fmt, ...);
log_error(fmt, ...);
```

### Build

You must install Apple's [Command Line Tools](https://developer.apple.com/download/more) as a minimal build environment, or Xcode as a full build environment in App Store.

#### Kernel extension

```shell
# cd into repository home
cd bsd_kext_log

# Default target is debug
make [release]
```

#### User space log daemon

```shell
cd bsd_kext_log/daemon

# Default target is debug
make [release]
```

### Test

After you compiled kext and daemon, you can load kext and run daemon to capture kernel messages.

Currently user space log daemon implementation is trivial, it only print logs directly into tty. Log persistence not yet implemented, we leave it to developers.

Sample way to test it:

```shell
cd bsd_kext_log
make
sudo cp -r bsd_kext_log.kext /tmp

sudo kextload -v 6 /tmp/bsd_kext_log.kext

cd daemon
make
./kextlog_daemon-debug
```

You'll see massive logs from KAuth subsystem, this experimental kext listen to several [KAuth scopes](https://developer.apple.com/library/archive/technotes/tn2127/_index.html) and push their logs into user space.

To stop the test, you should firstly terminate the daemon, and [kextunload(8)](x-man-page://8/kextunload) the kext.

### Caveats

* User space read buffer should over commit to kctl's `ctl_recvsize` so it can handle massive logs from kernel at one time.

* User space log handling should fast enough, since kernel messages will continue to push while user space not yet read.

	You may alternatively use two threads, one for read, the other to handle.

* The kctl accepts at most one client at a time, you need further effort to support multi-client(make nonsense?)

* When `log_printf` cannot push messages into user space daemon(like no user space client, client's buffer is full, kernel malloc failed, etc.), it'll instead print the log directly into system message buffer via [printf](http://xr.anadoxin.org/source/xref/macos-10.13.6-highsierra/xnu-4570.71.2/osfmk/kern/printf.c#853)(last possible resort).

### TODO

* Add `sysctl` entries for logging diagnostic/statistic purposes.

* Turn kctl's socket type into `SOCK_STREAM`, and test again.

* Test on other macOS versions.

### Contributing

Feel free to contribute this repository in any fashion.

Currently this kext tested under macOS 10.13.6(17G65) in a heavy file system event load, yet no warranty, use as your own risk!

### License

See [`LICENSE`](LICENSE) file for details.
