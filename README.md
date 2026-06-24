# ProcFS
A kernel-extension implementation of the `/proc` file system for macOS, exposing
running processes and threads as a filesystem with BSD- and Linux-compatible
per-process information. It began as a port of Kim Topley's proof-of-concept XNU
patch (see [Credits](#credits)) but has since diverged substantially.

Certain features and improvements are still planned and/or in development, but directory listing and the core per-process files work.

Tested on:

    - macOS 26 (Tahoe), Darwin 25.5.0, Apple Silicon (arm64e) — primary target
    - Builds as a universal binary (arm64e + x86_64)

> **Note on Apple Silicon:** under Pointer Authentication (PAC) the kernel's
> private symbols cannot be resolved from a kext. Where possible the affected
> features are forward-ported to work without those symbols (e.g. `fd/` and
> `threads/`); the few that cannot be (e.g. `tty`) degrade gracefully, returning
> `ENOTSUP` rather than failing the mount. See [Feature status](#feature-status)
> below.

## What is procfs?
*procfs* lets you view the processes running on a UNIX system as nodes in the file system, where each process is represented by a single directory named from its process id. Typically, the file system is mounted at `/proc`, so the directory for process 1 would be called `/proc/1`. Beneath a process’ directory are further directories and files that give more information about the process, such as its process id, its active threads, the files that it has open, and so on. *procfs* first appeared in an early version of AT&T’s UNIX and was later implemented in various forms in System V, BSD, Solaris and Linux. You can find a history of the implementation of *procfs* at https://en.wikipedia.org/wiki/Procfs.

In addition to letting you visualize running processes, *procfs* also allows some measure of control over them, at least to suitably privileged users. By writing specific data structures to certain files, you could do such things as set breakpoints and read and write process memory and registers. In fact, on some systems, this was how debugging facilities were provided. However, more modern operating systems do this differently, so some UNIX variants no longer include an implementation of *procfs*. In particular, macOS doesn’t provide *procfs* so, although it’s not strictly needed, I thought that implementing it would be an interesting side project. The code in this repository provides an implementation of *procfs* for macOS. You can use it to see what processes and threads are running on the system and what files they have open. Later, I plan to add more features, beginning with the ability to inspect a thread’s address space to see which executable it is running and what shared libraries it has loaded.

### Root directory

At the root of the file system, alongside the per-process directories, are a few
Linux-compatible files and helpers:

| Entry        | Summary                                                              |
|--------------|---------------------------------------------------------------------|
|`cpuinfo`     | Linux-style CPU information (text)                                   |
|`loadavg`     | Linux-style load averages (text; process count is real, load values are 0 on ARM64 — see below) |
|`partitions`  | Linux-style partition table (text; currently dummy values)          |
|`version`     | Kernel version string (text)                                        |
|`curproc`     | Symbolic link to the calling process's directory                    |
|`byname/`     | Directory of symbolic links, one per process, named by command name |

### Per-process files

Each directory named for a process id represents one process on the system. By default you can only see your own processes, although it is possible to set an option (`noprocperms`) when mounting the file system that will let you see and get details for every process. Obviously this is a security risk, so it’s not the default mode of operation. Within each process directory are the following files and two further directories. Most files contain binary structures rather than text, so they are intended to be used in applications rather than for direct human consumption. You’ll find definitions of the structures in this table in the file */usr/include/sys/proc_info.h*.

| File    | Summary                          | Structure                       |
|---------|----------------------------------|---------------------------------|
|`pid`      | Process id                       | `pid_t` (binary `int32`)         |
|`ppid`     | Parent process id                | `pid_t` (binary `int32`)         |
|`pgid`     | Process group id                 | `pid_t` (binary `int32`)         |
|`sid`      | Session id                       | `pid_t` (binary `int32`)         |
|`status`   | Basic process info               | `struct proc_bsdshortinfo`        |
|`taskinfo` | Info for the process’s Mach task | `struct proc_taskinfo` — **currently zeroed** (see Feature status) |
|`cmdline`  | Process command line             | text — **not yet implemented** (stub) |
|`tty`      | Controlling tty                  | string — **unavailable on ARM64** |
|`note`     | Write a note to the process (NetBSD-style) | write-only; read returns `EINVAL`. **Delivery not yet implemented** |

The `fd` directory contains one entry for each file that the process has open. Each entry is a directory that’s numbered for the corresponding file descriptor. Within each subdirectory you’ll find two files called `details` and `socket`. The `details` file contains a `vnode_fdinfowithpath` structure, which contains information about the file including its path name if it is a file system file. If the file is a socket endpoint, you can read a `socket_fdinfo` structure from the `socket` file.

The `threads` directory contains a subdirectory for each of the process’ threads, named by thread id. Each thread directory contains a single file called `info` that is meant to hold a `proc_threadinfo` structure. Thread *enumeration* works on Apple Silicon (the directory lists the real thread ids); the per-thread `info` *contents* are currently zeroed, pending a forward-port of the private `fill_taskthreadinfo`.

## Feature status

Verified with `test/test_features.sh`.

**Working (real data):**

  - Directory listing of the root and per-process directories via `ls`, `find`, `readdir(3)` and `getdirentries64(2)`
  - `version` — kernel version string
  - `cpuinfo` — Linux-style CPU information (some flag fields incomplete; see Issues)
  - `curproc` symlink and the `byname/` directory of per-process symlinks
  - Per-process `pid`, `ppid`, `pgid`, `sid` (binary `int32`)
  - Per-process `status` — `proc_bsdshortinfo` (pid/ppid/pgid, status, command
    name, real/effective/saved uids and gids, process flags)
  - `fd/` — enumerates the process's open file descriptors; per-fd `details`
    (`vnode_fdinfowithpath`) and `socket` (`socket_fdinfo`, common fields plus
    UNIX/IPv4 addresses)
  - `threads/` — enumerates the process's threads (one directory per thread id)

`fd/` and `threads/` required forward-porting work to function under PAC on Apple
Silicon rather than relying on the unavailable private KPIs — `fd/` walks the
process's file-descriptor table directly, and `threads/` enumerates threads via
the BSD `proc->p_uthlist` instead of the inaccessible Mach `task->threads` queue.

**Partially available (placeholder / incomplete data):**

  - `loadavg` — the process-count field is real, but the three load-average
    values read 0.00 on Apple Silicon. The kernel's `averunnable` global is not
    exported to kexts, is absent from the (stripped) kernel symbol table, and
    the `vm.loadavg` sysctl returns `EPERM` from kernel context, so there is no
    safe way to obtain the values.
  - `partitions` — emits a well-formed Linux-style table, but the values are
    dummy placeholders.
  - `taskinfo` and per-thread `threads/<tid>/info` — return correctly-sized but
    **all-zero** structures: they depend on the private `fill_taskprocinfo` /
    `fill_taskthreadinfo` symbols, which cannot be resolved under PAC and are not
    yet forward-ported.

**Present but not yet functional:**

  - `cmdline` — stub that returns the literal string `Feature not yet implemented.`
  - `note` — NetBSD-style node; reads return `EINVAL` as on NetBSD, but the node
    model is currently read-only (no `vnop_write`) and note delivery is
    unimplemented, so writing one is not yet possible.

**Unavailable on Apple Silicon (graceful degradation, not crashes):**

  - `tty` — returns `ENOTSUP`. The controlling terminal is reached through the
    SMR-protected `p->p_pgrp`, and every safe dereference bottoms out in symbols
    a kext cannot link (the `smr_*` read section, `proc_list_lock`, `proc_gettty`,
    `vnode_hold`/`vnode_drop`), so unlike `fd/` it cannot be forward-ported.

**Not yet present (planned — see TODO):**

  - Per-thread register/state files, process memory (`mem`) and address-space
    map (`map`), `auxv`, resource limits, and the broader set of Linux-style
    `/proc` files (`stat`, `meminfo`, `mounts`, `/proc/sys/`, …). Scaffolding for
    several of these exists in the source tree but is not yet wired into the
    node structure.

## How to build procfs
Build a universal (arm64e + x86_64) binary with:

    make ARCH=universal

To build and install in one step, use the provided install script (it runs
`make clean && make && sudo make install-only && make clean`):

    ./install ARCH=universal

`make install-only` copies `bin/procfs.kext` to `/Library/Extensions` and the
`bin/procfs.fs` mount helper to `/Library/Filesystems`, and sets the required
`root:wheel` ownership and `755` permissions on both.

Code signing is optional; the kext is ad-hoc signed by default. To sign with
your own certificate instead, edit `Makefile.inc` and set the `SIGNCERT`
variable to the identity in your keychain.

### Loading the kext (Apple Silicon, macOS 26)

Third-party kexts are loaded from the Auxiliary Kernel Collection (AuxKC). After
the first install you must approve the extension in **System Settings → Privacy
& Security** and reboot.

When **re-installing a rebuilt kext**, the build's code identity (cdhash)
changes, and a stale staging record will cause `kernelmanagerd` to reject it
with *"tried to insert an invalid codeful kernel extension in the restricted
lookup table."* Clear the staging area first, then the kext can be loaded into
the running kernel without a reboot:

    sudo umount ~/proc 2>/dev/null
    sudo kmutil unload -b com.beako.filesystems.procfs
    sudo kmutil clear-staging
    ./install ARCH=universal
    sudo kmutil load -p /Library/Extensions/procfs.kext
    kextstat | grep procfs

## Mounting
Open your Terminal application.

Now create an empty directory to use as your mount point, for example:

    mkdir ~/proc

Mount the file system:

    mount -t procfs proc proc

## Exploring the file system
Once mounted you can execute the `ls` command to list the contents:

    ls -l ~/proc

Or recursively:

    ls ~/proc/*/*/*/*

Note: Finder support has not yet been implemented so the contents of the proc folder will not be visible there. Currently only terminal is supported.

Likewise you can use the `cat` command to get the contents of a file:

    cat ~/proc/version

Most per-process files contain binary structures rather than text, so pipe them
through `hexdump` to read the raw contents:

    cat ~/proc/curproc/taskinfo | hexdump -C

## TODO:
 - Implement `cmdline` (reachable from a kext via `sysctl(KERN_PROCARGS2)`).
 - Populate `taskinfo` and per-thread `info` content (needs forward-ports of `fill_taskprocinfo` / `fill_taskthreadinfo`); enumeration already works.
 - Wire up the scaffolded but not-yet-exposed nodes: process memory (`mem`), address-space map (`map`), `auxv`, register state (`fpregs`/regs) and resource limits.
 - Implement `note` delivery (and a `vnop_write` path so the node is writable).
 - Fix per-node timestamps reported by `getattr` (currently show placeholder values in `ls -l`).
 - Make the code, function names, structures, etc. be more consistent with NetBSD's procfs for easier comparison and porting.
 - Implement more linux-compatible files a la NetBSD and FreeBSD (`stat`, `meminfo`, `mounts`, `/proc/sys/`, …).

## Issues
Currently known issues:

- On Apple Silicon, `tty` is unavailable because it depends on private kernel symbols that cannot be resolved under PAC (see [Feature status](#feature-status)). `fd/` and `threads/` previously had this limitation but have since been forward-ported and now work.
- `cmdline` is not yet implemented and returns a placeholder string.
- `taskinfo` and per-thread `threads/<tid>/info` are currently zeroed (they need the private `fill_taskprocinfo` / `fill_taskthreadinfo`); the surrounding `fd/` and `threads/` enumeration works.
- `note` is a NetBSD-style scaffold: reads return `EINVAL` and writing a note is not yet possible (no write path / no delivery).
- The `procfs_dopartitions` function in kext/procfs_linux.c is still in early stages of development so it will only return dummy values at the moment.
- Certain fields in `procfs_docpuinfo`, in kext/procfs_linux.c, such as `bugs` and `pm` have yet to be incorporated. Support for CPU flags for AMD CPUs is also still limited.

## Contributing and Bug Reporting
If you wish to contribute to this project then feel free to make a pull request. If you encounter any undocumented bugs then you may also file an issue under the "Issues" tab.

Contact me: sunnevanattsol@gmail.com

## Credits

This project builds on the work of others, with thanks to:

- **Kim Topley** — the original proof-of-concept `procfs` for XNU that this project started from.
  <https://github.com/kimtopley/ProcFS>
- **leiless** — the `libkext` kernel-extension helper library.
  <https://github.com/leiless/libkext>
- **Syncretic** — the `klookup` kernel symbol-resolution code, from the `latebloom` project (0BSD).
  <https://github.com/reenigneorcim/latebloom>
- **Apple** — the XNU kernel source, headers and reference implementation.
  <https://github.com/apple-oss-distributions/xnu>
- **The NetBSD project** — `procfs` design and reference implementation.
  <https://github.com/NetBSD/src>
- **The NextBSD project** — additional `procfs` reference.
  <https://github.com/NextBSD/NextBSD/tree/NextBSD-CURRENT>
- **The FreeBSD project** — `procfs`/`pseudofs` reference implementation.
  <https://github.com/freebsd/freebsd-src>
