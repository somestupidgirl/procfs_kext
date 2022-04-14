# ProcFS
A kext implementation of the /proc file system for OS X based on the XNU kernel patch by Kim Topley: https://github.com/kimtopley/ProcFS

## Important notice
This is still very much at an 'alpha' stage and not all features are yet working as they should. It *will* cause kernel panics in certain areas.

Tested on:

    - macOS Big Sur 11.6.3

## What is procfs?

*From Kim Topley's original Readme:*

*procfs* lets you view the processes running on a UNIX system as nodes in the file system, where each process is represented by a single directory named from its process id. Typically, the file system is mounted at `/proc`, so the directory for process 1 would be called `/proc/1`. Beneath a process’ directory are further directories and files that give more information about the process, such as its process id, its active threads, the files that it has open, and so on. *procfs* first appeared in an early version of AT&T’s UNIX and was later implemented in various forms in System V, BSD, Solaris and Linux. You can find a history of the implementation of *procfs* at https://en.wikipedia.org/wiki/Procfs.

In addition to letting you visualize running processes, *procfs* also allows some measure of control over them, at least to suitably privileged users. By writing specific data structures to certain files, you could do such things as set breakpoints and read and write process memory and registers. In fact, on some systems, this was how debugging facilities were provided. However, more modern operating systems do this differently, so some UNIX variants no longer include an implementation of *procfs*. In particular, OS X doesn’t provide *procfs* so, although it’s not strictly needed, I thought that implementing it would be an interesting side project. The code in this repository provides a very basic implementation of *procfs* for OS X. You can use it to see what processes and threads are running on the system and what files they have open. Later, I plan to add more features, neginning with the ability to inspect a thread’s address space to see which executable it is running and what shared libraries it has loaded.

Each directory in the left column represents one process on the system. By default you can only see your own processes, although it is possible to set an option when mounting the file system that will let you see and get details for every process. Obviously this is a security risk, so it’s not the default mode of operation. Within each process directory are seven files and two further directories, shown in the second column of the screenshot. All of the files can be read in the normal way, but the data that they contain is not text, so they are really intended to be used in applications rather than for direct human consumption. The following table summarizes what’s in each file. You’ll find definitions of the structures in this table in the file */usr/include/sys/proc_info.h*.

| File    | Summary                          | Structure                     |
|---------|----------------------------------|-------------------------------|
|`pid`    | Process id                       | `pid_t`                         |
|`ppid`     | Parent process id                | `pid_t`                         |
|`pgid`     | Process group id                 | `pid_t`                         |
|`sid`      | Session id                       | `pid_t`                         |
|`tty`      | Controlling tty                  | string, such as `/dev/tty000` |
|`info`     | Basic process info               | `struct proc_bsdinfo`           |
|`taskinfo` | Info for the process’s Mach task | `struct proc_taskinfo`          |

The `fd` directory contains one entry for each file that the process has open. Each entry is a directory that’s numbered for the corresponding file descriptor. Most processes will have at least entries 0, 1 and 2 for standard input, output and error respectively. Within each subdirectory you’ll find two files called `details` and `socket`. The `details` file contains a `vnode_fdinfowithpath` structure, which contains information about the file including its path name if it is a file system file. If the file is a socket endpoint, you can read a `socket_fdinfo` structure from the `socket` file.

The `threads` directory contains a subdirectory for each of the process’ threads. The process in the screenshot above has two threads with ids 550 and 1284. Each thread directory contains a single file called `info` the contains thread-specific information in the form of a `proc_threadinfo` structure.

## How to build procfs
To be able to build and install the kernel extension you must have a valid signing certificate in your keychain. Then you must open Makefile.inc and change the SIGNCERT variable to the email linked to your certificate. Once that's done, just open up a terminal window and execute:

    make && make install

You can use the provided install script:

    ./install

The script is just a simple routine of `make clean && make && make install && make clean`.

Once installed, open up your System Preferences, go to "Security", press "Allow" to load the kernel extension and reboot your system.

## Mounting
Open your Terminal application.

Now create an empty directory to use as your mount point, for example:

    mkdir ~/proc

Mount the file system:

    mount -t procfs proc proc

## Exploring the file system
Once mounted you can execute the `ls` command to list the contents:

    ls -l ~/proc

This should also work recursively:

    ls ~/proc/*/*/*

Likewise you can use the `cat` command to get the contents of a file:

    cat ~/proc/curproc/details

Finder support has not yet been implemented.

## TODO:
 - Add Finder support.
 - Make the code, function names, structures, etc. be more consistent with NetBSD's procfs for easier comparison and porting.
 - Implement more linux-compatible a la NetBSD and FreeBSD.
 - Improve AMD support.
 - Add support for ARM64.

## Issues
Currently known issues:

 - Functions `procfs_read_thread_info`, `procfs_read_fd_data` and `procfs_read_socket_data` are currently ridden with bugs.
 - A bug in procfs_read_sid_data will result in a kernel panic when executing cat on `/proc/<pid>/sid`.
 - A bug in procfs_read_tty_data will result in a kernel panic when executing cat on `/proc/<pid>/tty`.

## Contributing and Bug Reporting
If you wish to contribute to this project then feel free to make a pull request. If you encounter any undocumented bugs then you may also file an issue under the "Issues" tab.

Contact me: sunnevanattsol@gmail.com
