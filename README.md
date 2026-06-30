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
> private symbols cannot be linked from a kext, and the in-memory symbol table is
> jettisoned after boot. Affected features are recovered either by forward-porting
> (e.g. `fd/`, `threads/`, `cmdline`) or by resolving the needed private symbols
> from the on-disk kernel collection via libklookup (e.g. `tty`). Anything still
> out of reach degrades gracefully (returns `ENOTSUP`/empty) rather than failing
> the mount. See [Feature status](#feature-status) below.

## What is procfs?
*procfs* lets you view the processes running on a UNIX system as nodes in the file system, where each process is represented by a single directory named from its process id. Typically, the file system is mounted at `/proc`, so the directory for process 1 would be called `/proc/1`. Beneath a process’ directory are further directories and files that give more information about the process, such as its process id, its active threads, the files that it has open, and so on. *procfs* first appeared in an early version of AT&T’s UNIX and was later implemented in various forms in System V, BSD, Solaris and Linux. You can find a history of the implementation of *procfs* at https://en.wikipedia.org/wiki/Procfs.

In addition to letting you visualize running processes, *procfs* also allows some measure of control over them, at least to suitably privileged users. By writing specific data structures to certain files, you could do such things as set breakpoints and read and write process memory and registers. In fact, on some systems, this was how debugging facilities were provided. However, more modern operating systems do this differently, so some UNIX variants no longer include an implementation of *procfs*. In particular, macOS doesn’t provide *procfs* so, although it’s not strictly needed, I thought that implementing it would be an interesting side project. The code in this repository provides an implementation of *procfs* for macOS. You can use it to see what processes and threads are running on the system and what files they have open. Later, I plan to add more features, beginning with the ability to inspect a thread’s address space to see which executable it is running and what shared libraries it has loaded.

### Root directory

At the root of the file system, alongside the per-process directories, are a few
Linux-compatible files and helpers:

| Entry        | Summary                                                              |
|--------------|---------------------------------------------------------------------|
|`cpuinfo`     | Linux-style CPU information (text)                                   |
|`loadavg`     | Linux-style load averages (text; true values via the `procfsd` daemon, CPU-utilisation approximation as fallback — see below) |
|`meminfo`     | Linux-style memory summary (text; `MemFree` is the FreeBSD non-wired estimate on Apple Silicon — see below) |
|`partitions`  | Linux-style partition table (text; mounted block devices — see below) |
|`mtab`        | Linux-style mounted-filesystem table (`/etc/mtab` format: `device mountpoint fstype options 0 0`) |
|`stat`        | Linux-style kernel/system statistics (`cpu`/`cpuN` ticks, `btime`, `processes`; see below) |
|`vmstat`      | Linux-style virtual-memory statistics (daemon-backed `host_statistics64`; see below) |
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
|`taskinfo` | Info for the process’s Mach task | `struct proc_taskinfo` — exact via the `procfsd` daemon; falls back to the kext’s partial fill without it (see Feature status) |
|`cmdline`  | Process argument vector (NUL-separated, Linux format) | text |
|`tty`      | Controlling terminal device path (e.g. `/dev/ttys001`) | text |
|`note`     | Write a note to the process (NetBSD-style) | write-only; read returns `EINVAL`. **Delivery not yet implemented** |
|`limit`    | Process resource limits, one `<name> <cur> <max>` line per limit (`-1` = unlimited) | text |

The `fd` directory contains one entry for each file that the process has open. Each entry is a directory that’s numbered for the corresponding file descriptor. Within each subdirectory you’ll find two files called `details` and `socket`. The `details` file contains a `vnode_fdinfowithpath` structure, which contains information about the file including its path name if it is a file system file. If the file is a socket endpoint, you can read a `socket_fdinfo` structure from the `socket` file.

The `threads` directory contains a subdirectory for each of the process’ threads, named by thread id. Each thread directory contains a single file called `info` that holds a `proc_threadinfo` structure. Thread *enumeration* works on Apple Silicon (the directory lists the real thread ids), and the per-thread `info` *contents* are now supplied exactly by the `procfsd` daemon (`proc_pidinfo(PROC_PIDTHREADID64INFO)`); they read zero only when no daemon is connected (the private `fill_taskthreadinfo` is stripped from the arm64 kernel).

## Feature status

Verified with `test/test_features.sh`.

**Working (real data):**

  - Directory listing of the root and per-process directories via `ls`, `find`, `readdir(3)` and `getdirentries64(2)`
  - `version` — kernel version string
  - `cpuinfo` — Linux-style CPU information (some flag fields incomplete; see Issues)
  - `loadavg` — process count plus the true 1/5/15-minute load averages from the
    `procfsd` daemon (`getloadavg`); falls back to a CPU-utilisation approximation
    when no daemon is connected (see Apple Silicon note)
  - `meminfo` — Linux-style memory summary; `MemTotal` and `MemFree` are
    populated (`MemFree` via the FreeBSD non-wired estimate — see Apple Silicon note)
  - `partitions` — Linux-style table of mounted block devices (real major/minor,
    block counts and names; mounted volumes only — see note below)
  - `mtab` — Linux `/etc/mtab`-style table of every mounted filesystem
    (`device mountpoint fstype options 0 0`), via `vfs_iterate` + `vfs_statfs`
  - `stat` — Linux `/proc/stat`: per-CPU `cpu`/`cpuN` user/nice/system/idle
    ticks (from `processor_info`, the loadavg per-CPU source), `btime`
    (`kern.boottime`) and `processes`; interrupt/ctxt/fork counters read 0
  - `vmstat` — Linux `/proc/vmstat`: VM page counters from the `procfsd` daemon's
    `host_statistics64(HOST_VM_INFO64)`, mapped onto Linux keys (`nr_free_pages`,
    `pgpgin`/`pgpgout`, `pgfault`, …); zero without the daemon
  - `curproc` symlink and the `byname/` directory of per-process symlinks
  - Per-process `pid`, `ppid`, `pgid`, `sid` (binary `int32`)
  - Per-process `status` — `proc_bsdshortinfo` (pid/ppid/pgid, status, command
    name, real/effective/saved uids and gids, process flags)
  - `cmdline` — the process's argument vector, NUL-separated (the Linux
    `/proc/<pid>/cmdline` format); zombies and system processes report `(comm)`
  - `fd/` — enumerates the process's open file descriptors; per-fd `details`
    (`vnode_fdinfowithpath`) and `socket` (`socket_fdinfo`, common fields plus
    UNIX/IPv4 addresses)
  - `threads/` — enumerates the process's threads (one directory per thread id)
  - `task/` — the Linux name for the same per-thread view (`/proc/<pid>/task/<tid>`),
    one directory per thread id, with Linux-format per-thread files: `comm`,
    `stat`, `status`, `sched`, `maps` (and the binary `info`). Per-thread data
    (name, run state, user/system CPU time, priority, policy) comes from the
    `procfsd` daemon via `proc_pidinfo(PROC_PIDTHREADID64INFO)`; fields with no
    macOS source (Linux fault counters, CFS scheduler internals, register
    addresses, ctxt-switch counts) are reported as 0
  - `tty` — the process's controlling terminal device path (e.g. `/dev/ttys001`),
    empty when it has none
  - `limit` — the process's resource limits (FreeBSD `procfs_rlimit` format: one
    `<name> <cur> <max>` line per limit, `-1` for unlimited)
  - `mem` — the process's memory; the read offset is the virtual address (the
    NetBSD/Linux `mem` semantics). Only resident pages are returned (see Apple
    Silicon note)
  - `map` / `maps` — the process's virtual-memory regions (`map` in NetBSD
    procfs format, `maps` in Linux `/proc/<pid>/maps` format), with address
    ranges and protections (see Apple Silicon note)

`cmdline`, `fd/` and `threads/` required forward-porting work to function under
PAC on Apple Silicon rather than relying on the unavailable private KPIs: `fd/`
walks the process's file-descriptor table directly, `threads/` enumerates threads
via the BSD `proc->p_uthlist` instead of the inaccessible Mach `task->threads`
queue, and `cmdline` reads the target's user-stack arguments through its pmap
(the `KERN_PROCARGS2` `vm_map_copyin` path is `com.apple.kpi.private`).

`tty` takes a different route. Its accessor `proc_gettty` is
`com.apple.kpi.private` and reaches the terminal through the SMR-protected
`p->p_pgrp`, so it cannot be linked or safely forward-ported. Instead it is
resolved at load time from the on-disk kernel collection (libklookup, fed by the
`procfs_ksyms` staging helper run at install) and called directly — its SMR and
session locking run inside the kernel's own code, so the resolved call is safe.

`loadavg`'s load values come from the `procfsd` daemon's `getloadavg()` — the
kernel's true 1/5/15-minute averages — when it is connected. They are not
reachable from the kext itself: `averunnable`, `compute_averunnable`,
`host_statistics` and `processor_set_info` are all stripped from the kernel and
unexported. So without a daemon the node falls back to a CPU-utilisation
approximation: a `thread_call` samples per-CPU tick counts via the exported
`processor_info(PROCESSOR_CPU_LOAD_INFO)` (with a `processor_t` from the
libklookup-resolved `cpu_to_processor()`) every 5 seconds and feeds
`utilisation × ncpu` through the standard load-average EWMA. That approximation
tracks CPU utilisation rather than run-queue depth, so it saturates near the CPU
count and under-reports a genuinely overloaded machine; it reads `0.00` if
libklookup cannot resolve `cpu_to_processor` either.

`meminfo` reports `MemTotal` from the `hw.memsize` sysctl and `MemFree` using
FreeBSD's `linprocfs_domeminfo` estimate (`MemFree = MemTotal − wired`). The
wired-page count comes from the kernel's `vm_page_wire_count` global resolved
via libklookup, because the `vm.*` page-count sysctls are not readable from
kernel context and most `vm_page_*_count` globals are stripped on arm64.
`Cached`, `Buffers` and swap have no kernel-reachable source there and read 0
(`MemFree` reads 0 if libklookup cannot resolve the wired count).

`mem` reads the target's memory through its pmap (`get_task_pmap`,
`pmap_find_phys`, `ml_phys_read`) — the same physical-aperture path `cmdline`
uses — since the BSD faulting path (`vm_map_copyin`) is `com.apple.kpi.private`.
A consequence of not faulting is that only resident pages are returned: reading
an unmapped or paged-out address (including offset 0, the NULL page) returns
`EIO`, and a read stops cleanly at the first hole. Access is gated by the same
credential check as the rest of the filesystem.

`map` and `maps` enumerate the process's VM regions through the libklookup-
resolved `mach_vm_region()` (with `get_task_map()`): macOS exports no region-
enumeration KPI a kext may link, and the internal walkers are stripped on
arm64. `mach_vm_region()` takes the map's read lock itself, so this needs no
struct-walking or manual locking. Both nodes share one walk and differ only in
formatting (`map` NetBSD-style, `maps` Linux-style). Backing-file paths (the
trailing Linux column) are not resolved — the region's object→vnode is not
reachable here — so the device/inode/path columns read `00:00 0`.

`partitions` lists mounted block-device filesystems rather than raw disks.
Linux's `/proc/partitions` enumerates every block device, but on macOS that
information lives in IOKit, which a C VFS-only kext cannot reach. Instead the
node walks the mount list with `vfs_iterate()` and, for each `/dev/*` mount,
reports the major/minor from `vfs_statfs()`'s `f_fsid`, the 1 K block count from
`f_blocks × f_bsize`, and the device name — real data for every mounted volume
(on APFS each volume reports its shared container capacity). Unmounted
partitions are not shown.

### The `procfsd` daemon

Several fields are unreachable from a kext on Apple Silicon: the kernel
functions that produce them (`fill_taskprocinfo`, `task_info`, per-thread info,
the VM-statistics globals) are stripped from the kernel symbol table, and the
data lives in per-CPU/`recount` structures with no linkable accessor. The
`procfsd` userspace daemon supplies these via `libproc`'s `proc_pidinfo()` and
`host_statistics64()` — the same interfaces `top`/Activity Monitor use — over a
privileged `PF_SYSTEM` kernel control (`procfs_ctl.c`): a node read sends a
request and the daemon replies. So `taskinfo` (all 18 fields exact),
`task/<tid>/{info,comm,stat,status,sched}` and `vmstat` are fully populated when
the daemon runs, and fall back to the kext's best-effort values (or zero) when
it does not. The daemon also stages the libklookup symbol file at boot and,
when armed, loads the kext; see *Installing*. `taskinfo`'s `pti_resident_size`
is then the exact `phys_footprint` from the daemon (the kext's own estimate is
only the fallback). Without the daemon the per-thread `info` reads zero
(`fill_taskthreadinfo` is stripped from the arm64 kernel).

**Present but not yet functional:**

  - `note` — NetBSD-style node; reads return `EINVAL` as on NetBSD, but the node
    model is currently read-only (no `vnop_write`) and note delivery is
    unimplemented, so writing one is not yet possible.

**Not yet present (planned — see TODO):**

  - Per-thread register/state files (`regs`/`fpregs`), `auxv`, and further
    Linux-style `/proc` files (`/proc/sys/`, …).

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
 - Wire up the scaffolded but not-yet-exposed nodes: `auxv` and register state (`fpregs`/regs).
 - Implement `note` delivery (and a `vnop_write` path so the node is writable).
 - Fix per-node timestamps reported by `getattr` (currently show placeholder values in `ls -l`).
 - Make the code, function names, structures, etc. be more consistent with NetBSD's procfs for easier comparison and porting.
 - Implement more linux-compatible files a la NetBSD and FreeBSD (`/proc/sys/`, …).

## Issues
Currently known issues:

- On Apple Silicon, `cmdline`, `fd/`, `threads/` and `tty` previously required private kernel symbols unavailable under PAC; they now work (the first three forward-ported, `tty` via libklookup-resolved `proc_gettty`). `tty` depends on the `procfs_ksyms` staging helper having run (it does during `make install`); if the staged symbol file is missing or stale for the running kernel, `tty` falls back to `ENOTSUP`.
- `taskinfo` and per-thread `info` are populated by the `procfsd` daemon (`proc_pidinfo`); they read the kext fallback / zero only when no daemon is connected, since the private `fill_taskprocinfo` / `fill_taskthreadinfo` are stripped from the arm64 kernel.
- `note` is a NetBSD-style scaffold: reads return `EINVAL` and writing a note is not yet possible (no write path / no delivery).
- The `procfs_dopartitions` function in kext/procfs_linux.c lists mounted block-device filesystems (via `vfs_iterate`), not raw or unmounted disks — enumerating those needs IOKit, which a VFS-only kext can't reach.
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
- **Linus Henze** - for filesystem code pulled from his `Unrootless-Kext`
  <https://github.com/LinusHenze/Unrootless-Kext>
- **Acidanthera** - the `MacKernelSDK` macOS kernel SDK targeting various XNU versions.
  <https://github.com/acidanthera/MacKernelSDK>
- **Apple** — the XNU kernel source, headers and reference implementation.
  <https://github.com/apple-oss-distributions/xnu>
- **Apple** — the `libutil source and headers, needed by the mount_procfs tool.
  <https://github.com/apple-oss-distributions/libutil>
- **The NetBSD project** — `procfs` design and reference implementation.
  <https://github.com/NetBSD/src>
- **The NextBSD project** — additional `procfs` reference.
  <https://github.com/NextBSD/NextBSD/tree/NextBSD-CURRENT>
- **The FreeBSD project** — `procfs`/`pseudofs` reference implementation.
  <https://github.com/freebsd/freebsd-src>
