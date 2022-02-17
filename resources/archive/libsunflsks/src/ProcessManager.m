#include "../include/ProcessManager.h"
#include "../include/ProcessInfo.h"
#include "../include/FileInfo.h"
#include <signal.h>
#include <errno.h>
#include "../lib/libproc.h"
#include "../lib/proc_info.h"

static SunflsksProcessInfo* get_info_from_pid(pid_t pid);

@implementation SunflsksProcessManager {
    SunflsksProcessInfo* process_info;
}

-(SunflsksProcessManager*)initWithPID:(pid_t)pid {
    self = [super init];

    if (!self) {
        return nil;
    }

    process_info = get_info_from_pid(pid);
    return self;
}

-(ProcessOutput)kill {
    ProcessOutput output;

    if (kill([process_info pid], SIGTERM) != 0) {
        switch (errno) {
            case EINVAL:
                output = INVALID_SIGNAL;
                break;

            case EPERM:
                output = NOT_ALLOWED;
                break;

            case ESRCH:
                output = NONEXISTENT_PID;
                break;
        }
    }

    else {
        output = SUCCESS;
    }

    return output;
}

-(ProcessOutput)kill:(int)signal {
    ProcessOutput output;

    if (kill([process_info pid], signal) != 0) {
        switch (errno) {
            case EINVAL:
                output = INVALID_SIGNAL;
                break;

            case EPERM:
                output = NOT_ALLOWED;
                break;

            case ESRCH:
                output = NONEXISTENT_PID;
                break;
        }
    }

    else {
        output = SUCCESS;
    }

    return output;
}

-(NSArray<SunflsksFileInfo*>*)openFileDescriptors {
    NSMutableArray* ret = [NSMutableArray array];
    size_t buffersize = proc_pidinfo(process_info.pid, PROC_PIDLISTFDS, 0, NULL, 0);
    void* buffer = calloc(buffersize, 1);

    if (proc_pidinfo(process_info.pid, PROC_PIDLISTFDS, 0, buffer, buffersize) == -1) {
        return nil;
    }

    struct proc_fdinfo* fds = (struct proc_fdinfo*)buffer;
    int fd_count = buffersize / PROC_PIDLISTFD_SIZE;
    for (int i = 0; i < fd_count; i++) {
        [ret addObject:
            [[SunflsksFileInfo alloc] initWithProcFdInfo:fds[i] pid:process_info.pid]
        ];
    }

    return ret;
}

@end

static SunflsksProcessInfo* get_info_from_pid(pid_t pid) {
    void* buf = NULL;
	size_t buffer_size = 0;
	int err;
	bool finished = false;
	// MIB for getting all running processes
	int mib[4] = {
		CTL_KERN,
		KERN_PROC,
		KERN_PROC_PID,
        pid,
	};
	do {
		// Divide sizeof mib by sizeof *mib (which is int) to get the total number of items in mib[]
		err = sysctl(mib, sizeof(mib) / sizeof(int), NULL, &buffer_size, NULL, 0);
		if (err == -1) {
			err = errno;
		}

		else if (err == 0) {
			buf = malloc(buffer_size);
			if (buf == NULL) {
				err = errno;
			}
		}

		if (err == 0) {
			err = sysctl(mib, sizeof(mib) / sizeof(int), buf, &buffer_size, NULL, 0);

			if (err == -1) {
				err = errno;
			}

			else if (err == 0) {
				finished = true;
			}

			else if (errno == ENOMEM) {
				if (buf != NULL) {
					free(buf);
				}
			}
		}
	} while (!finished);

    struct kinfo_proc* proclist = (struct kinfo_proc*)buf;

    return [[SunflsksProcessInfo alloc] initWithProc:proclist[0].kp_proc];
}