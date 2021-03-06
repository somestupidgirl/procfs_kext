//
//  mount_procfs.c
//  mount_procfs
//
//  Created by Kim Topley on 12/1/15.
//
// The procfs-specific mount command, which must be installed in
// the /sbin directory. In addition to the standard mount options,
// the option "procperms" and its inverse ("noprocperms") is
// supported. When "noprocperms" is used, all files and directories
// in the mounted file system have access permissions that allow
// any process to read them. This, of course, is a huge security
// loophole, so it should only be used for testing. The default is
// "procperms" (which is secure).
//

#ifndef __FSBUNDLE__
#define __FSBUNDLE__
#endif

#include <sys/mount.h>

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <miscfs/procfs/procfs.h>

#include <libutil/mntopts.h>

// Forward declarations of local functions.
static void usage(char *name);

// Verbose logging flag and logging level for syslog(3)
static const int PROCFS_SYSLOG_LEVEL = LOG_INFO;
static _Bool verbose = FALSE;

// Mount options.
static struct mntopt mopts[] = {
    MOPT_STDOPTS,
    MOPT_PROCFS,
};

int main(int argc, char *argv[])
{
    /* -- Argument processing. Extracts mount options -- */
    char *prog_name = basename(argv[0]);

    // Default generic mount options and procfs options, which can be overridden
    // using the -o option.
    int generic_options = MNT_NOEXEC | MNT_NOSUID;
    int procfs_options = 0;

    opterr = 0;  // Silence default messages from getopt()
    int option;
    while ((option = getopt(argc, argv, "vo:?h")) != -1) {
        switch (option) {
        case '?':
            /*FALLTHRU*/
        case 'h':
            usage(prog_name);
            /*NOTREACHED*/
                
        case 'v':
            verbose = TRUE;
            break;
                
        case 'o': {
            mntoptparse_t mntops = getmntopts(optarg, mopts, &generic_options, &procfs_options);
            freemntopts(mntops);
            break;
        }
                
        default: // Unrecognized option.
            usage(prog_name);
            /*NOTREACHED*/
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 2) {
        // Expecting special and mount point arguments.
        usage(argv[0]);
        /*NOTREACHED*/
    }

    /* -- Mount the file system -- */
    pfsmount_args_t mount_args;
    mount_args.mnt_options = procfs_options;

    char *mntdir = argv[1];
    if (verbose) {
        syslog(PROCFS_SYSLOG_LEVEL, "%s: Mounting procfs on %s", prog_name, mntdir);
    }

    int result = mount("procfs", mntdir, generic_options, &mount_args);
    if (result < 0) {
        fprintf(stderr, "%s: Failed to mount procfs on %s: %s\n", prog_name, mntdir, strerror(errno));
    }

    if (verbose) {
        if (result == 0) {
            syslog(PROCFS_SYSLOG_LEVEL, "%s: mount completed", prog_name);
        } else {
            syslog(PROCFS_SYSLOG_LEVEL, "%s: mount failed: %s", prog_name, strerror(errno));
        }
    }
    
    return result == 0 ? 0 : 1;
}

/*
 * Prints a usage message and exits.
 */
static void
usage(char *name) {
    fprintf(stderr, "%s: usage: %s [-o options] special mountpoint\n", name, name);
    fprintf(stderr, "Options are:\n");
    fprintf(stderr, "     procperms\t\tConfigures process nodes so that only process owner can view process info. On by default.\n");
    fprintf(stderr, "     noprocperms\tDisables procperms. Use with extreme caution - this is a security risk.\n");
    fprintf(stderr, "     -v\t\t\tEnables verbose logging of mount operation to syslog.\n");
    fprintf(stderr, "     -?, -h\t\tPrints this usage message and exits.\n");
    fprintf(stderr, "Example: mount -t %s -o procperms,-v %s /proc\n", "procfs", "procfs");
    
    exit(1);
    /* NOTREACHED */
}
