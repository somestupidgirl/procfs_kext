#ifndef procfs_internal_h
#define procfs_internal_h

#include <stdatomic.h>
#include <stdbool.h>

#include <os/base.h>

#include <kern/locks.h>

#include <sys/event.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>

#define M_CACHE             26

#define MAXTHREADNAMESIZE   64
#define MAXCOMLEN           16

typedef char command_t[MAXCOMLEN + 1];
typedef char proc_name_t[2*MAXCOMLEN + 1];
typedef uint64_t pending_io_t;

struct os_refcnt;
typedef struct os_refcnt os_refcnt_t;

/* type of the internal counter */
typedef uint32_t os_ref_count_t;
typedef _Atomic(os_ref_count_t) os_ref_atomic_t;

os_ref_count_t os_ref_get_count(struct os_refcnt *rc);

// =================
// sys/file_internal.h

struct kevent_qos_s;

__options_decl(fileproc_vflags_t, unsigned int, {
    FPV_NONE        = 0,
    FPV_DRAIN       = 0x01,
});

struct fileproc {
	unsigned int fp_flags;
	_Atomic fileproc_vflags_t fp_vflags;
	os_refcnt_t *fp_iocount;
	struct fileglob *fp_glob;
	void *fp_wset;
};

typedef enum {
    DTYPE_VNODE     = 1,    /* file */
    DTYPE_SOCKET,           /* communications endpoint */
    DTYPE_PSXSHM,           /* POSIX Shared memory */
    DTYPE_PSXSEM,           /* POSIX Semaphores */
    DTYPE_KQUEUE,           /* kqueue */
    DTYPE_PIPE,             /* pipe */
    DTYPE_FSEVENTS,         /* fsevents */
    DTYPE_ATALK,            /* (obsolete) */
    DTYPE_NETPOLICY,        /* networking policy */
} file_type_t;

struct fileops {
    file_type_t     fo_type;        /* descriptor type */
    int           (*fo_read)        (struct fileproc *fp, struct uio *uio, int flags, vfs_context_t ctx);
    int           (*fo_write)       (struct fileproc *fp, struct uio *uio, int flags, vfs_context_t ctx);
#define FOF_OFFSET      0x00000001  /* offset supplied to vn_write */
#define FOF_PCRED       0x00000002  /* cred from proc, not current thread */
    int           (*fo_ioctl)       (struct fileproc *fp, u_long com, caddr_t data, vfs_context_t ctx);
    int           (*fo_select)      (struct fileproc *fp, int which, void *wql, vfs_context_t ctx);
    int           (*fo_close)       (struct fileglob *fg, vfs_context_t ctx);
    int           (*fo_kqfilter)    (struct fileproc *fp, struct knote *, struct kevent_qos_s *);
    int           (*fo_drain)       (struct fileproc *fp, vfs_context_t ctx);
};

struct fileglob {
    LIST_ENTRY(fileglob) f_msglist;     /* list of files in unix messages */
    uint32_t             fg_flag;       /* (atomic) see fcntl.h */
    os_ref_atomic_t      fg_count;      /* reference count */
    uint32_t             fg_msgcount;   /* references from message queue */
    int32_t              fg_lflags;     /* file global flags */
    kauth_cred_t         fg_cred;       /* credentials associated with descriptor */
    const struct fileops *fg_ops;
    off_t                fg_offset;
    void                *fg_data;       /* vnode or socket or SHM or semaphore */
    void                *fg_vn_data;    /* Per fd vnode data, used for directories */
    lck_mtx_t           *fg_lock;
};

int fp_drop(struct proc *p, int fd, struct fileproc *fp, int locked);
int fp_getfvpandvid(struct proc *p, int fd, struct fileproc **resultfp, struct vnode  **resultvp, uint32_t * vidp);
int fp_getfsock(struct proc *p, int fd, struct fileproc **resultfp, socket_t  *results); // reimplemented in procfs_internal.c


// =================
// sys/filedesc.h

#define UF_RESERVED        0x04                   /* open pending / in progress */

struct filedesc {
	struct fileproc     **fd_ofiles;              /* file structures for open files */
	lck_mtx_t            *fd_kqhashlock;          /* lock for dynamic kqueue hash */
	u_long                fd_kqhashmask;          /* size of dynamic kqueue hash */
	struct kqwllist      *fd_kqhash;              /* hash table for dynamic kqueues */
	struct kqworkq       *fd_wqkqueue;            /* the workq kqueue */
	char                 *fd_ofileflags;          /* per-process open file flags */
	struct vnode         *fd_cdir;                /* current directory */
	struct vnode         *fd_rdir;                /* root directory */
	int                   fd_nfiles;              /* number of open files allocated */
	int                   fd_lastfile;            /* high-water mark of fd_ofiles */
	int                   fd_freefile;            /* approx. next free file */
	mode_t                fd_cmask;               /* mask for file creation */
	int                   fd_flags;
	int                   fd_knlistsize;          /* size of knlist */
	struct klist         *fd_knlist;              /* list of attached knotes */
	u_long                fd_knhashmask;          /* size of knhash */
	struct klist         *fd_knhash;              /* hash table for attached knotes */
	lck_mtx_t            *fd_knhashlock;          /* lock for hash table for attached knotes */
};


// =================
// sys/mount_internal.h

TAILQ_HEAD(vnodelst, vnode);

struct mount {
    TAILQ_ENTRY(mount)    mnt_list;               /* mount list */
    int32_t               mnt_count;              /* reference on the mount */
    lck_mtx_t            *mnt_mlock;              /* mutex that protects mount point */
    const struct vfsops  *mnt_op;                 /* operations on fs */
    struct vfstable      *mnt_vtable;             /* configuration info */
    struct vnode         *mnt_vnodecovered;       /* vnode we mounted on */
    struct vnodelst      *mnt_vnodelist;          /* list of vnodes this mount */
    struct vnodelst      *mnt_workerqueue;        /* list of vnodes this mount */
    struct vnodelst      *mnt_newvnodes;          /* list of vnodes this mount */
    uint32_t              mnt_flag;               /* flags */
    uint32_t              mnt_kern_flag;          /* kernel only flags.  NOTE: See mnt_supl_kern_flags below! */
    uint32_t              mnt_compound_ops;       /* Available compound operations */
    uint32_t              mnt_lflag;              /* mount life cycle flags */
    uint32_t              mnt_maxsymlinklen;      /* max size of short symlink */
    struct vfsstatfs      mnt_vfsstat;            /* cache of filesystem stats */
    qaddr_t               mnt_data;               /* private data */
    uint32_t              mnt_maxreadcnt;         /* Max. byte count for read */
    uint32_t              mnt_maxwritecnt;        /* Max. byte count for write */
    uint32_t              mnt_segreadcnt;         /* Max. segment count for read */
    uint32_t              mnt_segwritecnt;        /* Max. segment count for write */
    uint32_t              mnt_maxsegreadsize;     /* Max. segment read size  */
    uint32_t              mnt_maxsegwritesize;    /* Max. segment write size */
    uint32_t              mnt_alignmentmask;      /* Mask of bits that aren't addressable via DMA */
    uint32_t              mnt_devblocksize;       /* the underlying device block size */
    uint32_t              mnt_ioqueue_depth;      /* the maxiumum number of commands a device can accept */
    uint32_t              mnt_ioscale;            /* scale the various throttles/limits imposed on the amount of I/O in flight */
    uint32_t              mnt_ioflags;            /* flags for  underlying device */
    uint32_t              mnt_minsaturationbytecount; /* if non-zero, mininum amount of writes (in bytes) needed to max out throughput */
    pending_io_t         *mnt_pending_write_size __attribute__((aligned(sizeof(pending_io_t)))); /* byte count of pending writes */
    pending_io_t         *mnt_pending_read_size  __attribute__((aligned(sizeof(pending_io_t)))); /* byte count of pending reads */
    struct timeval        mnt_last_write_issued_timestamp;
    struct timeval        mnt_last_write_completed_timestamp;
    int64_t               mnt_max_swappin_available;
    lck_rw_t             *mnt_rwlock;             /* mutex readwrite lock */
    lck_mtx_t            *mnt_renamelock;         /* mutex that serializes renames that change shape of tree */
    vnode_t              *mnt_devvp;              /* the device mounted on for local file systems */
    uint32_t              mnt_devbsdunit;         /* the BSD unit number of the device */
    uint64_t              mnt_throttle_mask;      /* the throttle mask of what devices will be affected by I/O from this mnt */
    void                 *mnt_throttle_info;      /* used by the throttle code */
    int32_t               mnt_crossref;           /* refernces to cover lookups  crossing into mp */
    int32_t               mnt_iterref;            /* refernces to cover iterations; drained makes it -ve  */
    uid_t                 mnt_fsowner;
    gid_t                 mnt_fsgroup;
    struct label         *mnt_mntlabel;           /* MAC mount label */
    struct label         *mnt_fslabel;            /* MAC default fs label */
    vnode_t              *mnt_realrootvp;
    uint32_t              mnt_realrootvp_vid;
    uint32_t              mnt_generation;
    int                   mnt_authcache_ttl;
    char                  fstypename_override[MFSTYPENAMELEN];
    uint32_t              mnt_iobufinuse;
    void                 *mnt_disk_conditioner_info;
    lck_mtx_t            *mnt_iter_lock;          /* mutex that protects iteration of vnodes */
    uint64_t              mnt_mount_id;           /* system-wide unique mount ID */
    uint32_t              mnt_supl_kern_flag;     /* Supplemental kernel-only mount flags */
};

#define VFC_VFS64BITREADY       0x100
#define VFC_VFSNOMACLABEL       0x1000

// =================
// sys/proc.h

task_t proc_task(proc_t);


// =================
// sys/proc_info.h

struct proc_bsdinfo {
    uint32_t              pbi_flags;              /* 64bit; emulated etc */
    uint32_t              pbi_status;
    uint32_t              pbi_xstatus;
    uint32_t              pbi_pid;
    uint32_t              pbi_ppid;
    uid_t                 pbi_uid;
    gid_t                 pbi_gid;
    uid_t                 pbi_ruid;
    gid_t                 pbi_rgid;
    uid_t                 pbi_svuid;
    gid_t                 pbi_svgid;
    uint32_t              rfu_1;                  /* reserved */
    char                  pbi_comm[MAXCOMLEN];
    char                  pbi_name[2 * MAXCOMLEN]; /* empty if no name is registered */
    uint32_t              pbi_nfiles;
    uint32_t              pbi_pgid;
    uint32_t              pbi_pjobc;
    uint32_t              e_tdev;                 /* controlling tty dev */
    uint32_t              e_tpgid;                /* tty process group id */
    int32_t               pbi_nice;
    uint64_t              pbi_start_tvsec;
    uint64_t              pbi_start_tvusec;
};

struct proc_taskinfo {
    uint64_t              pti_virtual_size;       /* virtual memory size (bytes) */
    uint64_t              pti_resident_size;      /* resident memory size (bytes) */
    uint64_t              pti_total_user;         /* total time */
    uint64_t              pti_total_system;
    uint64_t              pti_threads_user;       /* existing threads only */
    uint64_t              pti_threads_system;
    int32_t               pti_policy;             /* default policy for new threads */
    int32_t               pti_faults;             /* number of page faults */
    int32_t               pti_pageins;            /* number of actual pageins */
    int32_t               pti_cow_faults;         /* number of copy-on-write faults */
    int32_t               pti_messages_sent;      /* number of messages sent */
    int32_t               pti_messages_received;  /* number of messages received */
    int32_t               pti_syscalls_mach;      /* number of mach system calls */
    int32_t               pti_syscalls_unix;      /* number of unix system calls */
    int32_t               pti_csw;                /* number of context switches */
    int32_t               pti_threadnum;          /* number of threads in the task */
    int32_t               pti_numrunning;         /* number of running threads */
    int32_t               pti_priority;           /* task priority*/
};

struct proc_threadinfo {
    uint64_t              pth_user_time;          /* user run time */
    uint64_t              pth_system_time;        /* system run time */
    int32_t               pth_cpu_usage;          /* scaled cpu usage percentage */
    int32_t               pth_policy;             /* scheduling policy in effect */
    int32_t               pth_run_state;          /* run state (see below) */
    int32_t               pth_flags;              /* various flags (see below) */
    int32_t               pth_sleep_time;         /* number of seconds that thread */
    int32_t               pth_curpri;             /* cur priority*/
    int32_t               pth_priority;           /*  priority*/
    int32_t               pth_maxpriority;        /* max priority*/
    char                  pth_name[MAXTHREADNAMESIZE]; /* thread name, if any */
};

struct proc_fileinfo {
	uint32_t              fi_openflags;
	uint32_t              fi_status;
	off_t                 fi_offset;
	int32_t               fi_type;
	uint32_t              fi_guardflags;
};

struct vinfo_stat {
    uint32_t              vst_dev;        /* [XSI] ID of device containing file */
    uint16_t              vst_mode;       /* [XSI] Mode of file (see below) */
    uint16_t              vst_nlink;      /* [XSI] Number of hard links */
    uint64_t              vst_ino;        /* [XSI] File serial number */
    uid_t                 vst_uid;        /* [XSI] User ID of the file */
    gid_t                 vst_gid;        /* [XSI] Group ID of the file */
    int64_t               vst_atime;      /* [XSI] Time of last access */
    int64_t               vst_atimensec;  /* nsec of last access */
    int64_t               vst_mtime;      /* [XSI] Last data modification time */
    int64_t               vst_mtimensec;  /* last data modification nsec */
    int64_t               vst_ctime;      /* [XSI] Time of last status change */
    int64_t               vst_ctimensec;  /* nsec of last status change */
    int64_t               vst_birthtime;  /*  File creation time(birth)  */
    int64_t               vst_birthtimensec;      /* nsec of File creation time */
    off_t                 vst_size;       /* [XSI] file size, in bytes */
    int64_t               vst_blocks;     /* [XSI] blocks allocated for file */
    int32_t               vst_blksize;    /* [XSI] optimal blocksize for I/O */
    uint32_t              vst_flags;      /* user defined flags for file */
    uint32_t              vst_gen;        /* file generation number */
    uint32_t              vst_rdev;       /* [XSI] Device ID */
    int64_t               vst_qspare[2];  /* RESERVED: DO NOT USE! */
};

struct sockbuf_info {
    uint32_t                sbi_cc;
    uint32_t                sbi_hiwat;                      /* SO_RCVBUF, SO_SNDBUF */
    uint32_t                sbi_mbcnt;
    uint32_t                sbi_mbmax;
    uint32_t                sbi_lowat;
    short                   sbi_flags;
    short                   sbi_timeo;
};

enum {
    SOCKINFO_GENERIC        = 0,
    SOCKINFO_IN             = 1,
    SOCKINFO_TCP            = 2,
    SOCKINFO_UN             = 3,
    SOCKINFO_NDRV           = 4,
    SOCKINFO_KERN_EVENT     = 5,
    SOCKINFO_KERN_CTL       = 6,
    SOCKINFO_VSOCK          = 7,
};

struct socket_info {
    struct vinfo_stat                       soi_stat;
    uint64_t                                soi_so;         /* opaque handle of socket */
    uint64_t                                soi_pcb;        /* opaque handle of protocol control block */
    int                                     soi_type;
    int                                     soi_protocol;
    int                                     soi_family;
    short                                   soi_options;
    short                                   soi_linger;
    short                                   soi_state;
    short                                   soi_qlen;
    short                                   soi_incqlen;
    short                                   soi_qlimit;
    short                                   soi_timeo;
    u_short                                 soi_error;
    uint32_t                                soi_oobmark;
    struct sockbuf_info                     soi_rcv;
    struct sockbuf_info                     soi_snd;
    int                                     soi_kind;
    uint32_t                                rfu_1;          /* reserved */
    union {
        struct in_sockinfo      *pri_in;                 /* SOCKINFO_IN */
        struct tcp_sockinfo     *pri_tcp;                /* SOCKINFO_TCP */
        struct un_sockinfo      *pri_un;                 /* SOCKINFO_UN */
        struct ndrv_info        *pri_ndrv;               /* SOCKINFO_NDRV */
        struct kern_event_info  *pri_kern_event;         /* SOCKINFO_KERN_EVENT */
        struct kern_ctl_info    *pri_kern_ctl;           /* SOCKINFO_KERN_CTL */
        struct vsock_sockinfo   *pri_vsock;              /* SOCKINFO_VSOCK */
    }                                       soi_proto;
};

struct vnode_info {
	struct vinfo_stat     vi_stat;
	int                   vi_type;
	int                   vi_pad;
	fsid_t                vi_fsid;
};

struct vnode_info_path {
    struct vnode_info     vip_vi;
    char                  vip_path[MAXPATHLEN]; /* tail end of it  */
};

struct vnode_fdinfowithpath {
	struct proc_fileinfo    pfi;
	struct vnode_info_path *pvip;
};

struct socket_fdinfo {
    struct proc_fileinfo    pfi;
    socket_t               *psi;
};

extern int fill_socketinfo(socket_t so, struct socket_info *si);


// =================
// sys/proc_internal.h

struct session {
    int                     s_count;                /* Ref cnt; pgrps in session. (LL) */
    struct proc            *s_leader;               /* Session leader.(static) */
    struct vnode           *s_ttyvp;                /* Vnode of controlling terminal.(SL) */
    int                     s_ttyvid;               /* Vnode id of the controlling terminal (SL) */
    struct tty             *s_ttyp;                 /* Controlling terminal. (SL + ttyvp != NULL) */
    pid_t                   s_ttypgrpid;            /* tty's pgrp id */
    pid_t                   s_sid;                  /* Session ID (static) */
    char                    s_login[MAXLOGNAME];    /* Setlogin() name.(SL) */
    int                     s_flags;                /* Session flags (s_mlock)  */
    LIST_ENTRY(session)     s_hash;                 /* Hash chain.(LL) */
    lck_mtx_t              *s_mlock;                /* mutex lock to protect session */
    int                     s_listflags;
};

struct proc
{
    LIST_ENTRY(proc)      p_list;                 /* List of all processes. */
    void                 *task;                   /* corresponding task (static)*/
    struct proc          *p_pptr;                 /* Pointer to parent process.(LL) */
    pid_t                 p_ppid;                 /* process's parent pid number */
    pid_t                 p_original_ppid;        /* process's original parent pid number, doesn't change if reparented */
    pid_t                 p_pgrpid;               /* process group id of the process (LL)*/
    uid_t                 p_uid;
    gid_t                 p_gid;
    uid_t                 p_ruid;
    gid_t                 p_rgid;
    uid_t                 p_svuid;
    gid_t                 p_svgid;
    uint64_t              p_uniqueid;             /* process unique ID - incremented on fork/spawn/vfork, remains same across exec. */
    uint64_t              p_puniqueid;            /* parent's unique ID - set on fork/spawn/vfork, doesn't change if reparented. */
    lck_mtx_t            *p_mlock;                /* mutex lock for proc */
    pid_t                 p_pid;                  /* Process identifier. (static)*/
    char                  p_stat;                 /* S* process status. (PL)*/
    char                  p_shutdownstate;
    char                  p_kdebug;               /* P_KDEBUG eq (CC)*/
    char                  p_btrace;               /* P_BTRACE eq (CC)*/
    LIST_ENTRY(proc)      p_pglist;               /* List of processes in pgrp.(PGL) */
    LIST_ENTRY(proc)      p_sibling;              /* List of sibling processes. (LL)*/
    LIST_HEAD(, proc)     p_children;             /* Pointer to list of children. (LL)*/
    TAILQ_HEAD(, uthread) p_uthlist;              /* List of uthreads  (PL) */
    LIST_ENTRY(proc)      p_hash;                 /* Hash chain. (LL)*/
    lck_mtx_t            *p_fdmlock;              /* proc lock to protect fdesc */
    lck_mtx_t            *p_ucred_mlock;          /* mutex lock to protect p_ucred */
    kauth_cred_t         *p_ucred;                /* Process owner's identity. (PUCL) */
    struct filedesc       p_fd;                   /* Ptr to open files structure. (PFDL) */
    struct pstats        *p_stats;                /* Accounting/statistics (PL). */
    struct plimit        *p_limit;                /* Process limits.(PL) */
    struct sigacts       *p_sigacts;              /* Signal actions, state (PL) */
    lck_spin_t           *p_slock;                /* spin lock for itimer/profil protection */
    int                   p_siglist;              /* signals captured back from threads */
    unsigned int          p_flag;                 /* P_* flags. (atomic bit ops) */
    unsigned int          p_lflag;                /* local flags  (PL) */
    unsigned int          p_listflag;             /* list flags (LL) */
    unsigned int          p_ladvflag;             /* local adv flags (atomic) */
    int                   p_refcount;             /* number of outstanding users(LL) */
    int                   p_childrencnt;          /* children holding ref on parent (LL) */
    int                   p_parentref;            /* children lookup ref on parent (LL) */
    pid_t                 p_oppid;                /* Save parent pid during ptrace. XXX */
    u_int                 p_xstat;                /* Exit status for wait; also stop signal. */
    struct itimerval     *p_realtimer;            /* Alarm timer. (PSL) */
    struct timeval       *p_rtime;                /* Real time.(PSL)  */
    struct itimerval     *p_vtimer_user;          /* Virtual timers.(PSL)  */
    struct itimerval     *p_vtimer_prof;          /* (PSL) */
    struct timeval       *p_rlim_cpu;             /* Remaining rlim cpu value.(PSL) */
    int                   p_debugger;             /*  NU 1: can exec set-bit programs if suser */
    boolean_t             sigwait;                /* indication to suspend (PL) */
    void                 *sigwait_thread;         /* 'thread' holding sigwait(PL)  */
    void                 *exit_thread;            /* Which thread is exiting(PL)  */
    void                 *p_vforkact;             /* activation running this vfork proc)(static)  */
    int                   p_vforkcnt;             /* number of outstanding vforks(PL)  */
    int                   p_fpdrainwait;          /* (PFDL) */
    pid_t                 si_pid;                 /* (PL) */
    u_int                 si_status;              /* (PL) */
    u_int                 si_code;                /* (PL) */
    uid_t                 si_uid;                 /* (PL) */
    void *                vm_shm;                 /* (SYSV SHM Lock) for sysV shared memory */
#define p_startcopy       p_argslen
    u_int                 p_argslen;              /* Length of process arguments. */
    int                   p_argc;                 /* saved argc for sysctl_procargs() */
    user_addr_t           user_stack;             /* where user stack was allocated */
    struct vnode         *p_textvp;               /* Vnode of executable. */
    off_t                 p_textoff;              /* offset in executable vnode */
    sigset_t              p_sigmask;              /* DEPRECATED */
    sigset_t              p_sigignore;            /* Signals being ignored. (PL) */
    sigset_t              p_sigcatch;             /* Signals being caught by user.(PL)  */
    u_char                p_priority;             /* (NU) Process priority. */
    u_char                p_resv0;                /* (NU) User-priority based on p_cpu and p_nice. */
    char                  p_nice;                 /* Process "nice" value.(PL) */
    u_char                p_resv1;                /* (NU) User-priority based on p_cpu and p_nice. */
    command_t             p_comm;
    proc_name_t           p_name;                 /* can be changed by the process */
    uint8_t               p_xhighbits;            /* Stores the top byte of exit status to avoid truncation*/
    pid_t                 p_contproc;             /* last PID to send us a SIGCONT (PL) */
    struct  pgrp *        p_pgrp;                 /* Pointer to process group. (LL) */
    uint32_t              p_csflags;              /* flags for codesign (PL) */
    uint32_t              p_pcaction;             /* action  for process control on starvation */
    uint8_t               p_uuid[16];             /* from LC_UUID load command */
    cpu_type_t            p_cputype;
    cpu_subtype_t         p_cpusubtype;
    uint8_t              *syscall_filter_mask;    /* syscall filter bitmask (length: nsysent bits) */
    uint32_t              p_platform;
    uint32_t              p_min_sdk;
    uint32_t              p_sdk;
#define p_endcopy         p_aio_total_count
    int                   p_aio_total_count;      /* all allocated AIO requests for this proc */
    TAILQ_HEAD(, aio_workq_entry ) p_aio_activeq; /* active async IO requests */
    TAILQ_HEAD(, aio_workq_entry ) p_aio_doneq;   /* completed async IO requests */
    struct klist         *p_klist;                /* knote list (PL ?)*/
    struct rusage_superset *p_ru;                /* Exit information. (PL) */
    thread_t              p_signalholder;
    thread_t              p_transholder;
    int                   p_sigwaitcnt;
    /* DEPRECATE following field  */
    u_short               p_acflag;               /* Accounting flags. */
    volatile u_short      p_vfs_iopolicy;         /* VFS iopolicy flags. (atomic bit ops) */
    user_addr_t           p_threadstart;          /* pthread start fn */
    user_addr_t           p_wqthread;             /* pthread workqueue fn */
    int     p_pthsize;                            /* pthread size */
    uint32_t              p_pth_tsd_offset;       /* offset from pthread_t to TSD for new threads */
    user_addr_t           p_stack_addr_hint;      /* stack allocation hint for wq threads */
    struct workqueue     *_Atomic p_wqptr;        /* workq ptr */
    struct timeval       *p_start;                /* starting time */
    void *                p_rcall;
    int                   p_ractive;
    int                   p_idversion;            /* version of process identity */
    void *                p_pthhash;              /* pthread waitqueue hash */
    volatile uint64_t     was_throttled __attribute__((aligned(8))); /* Counter for number of throttled I/Os */
    volatile uint64_t     did_throttle __attribute__((aligned(8)));  /* Counter for number of I/Os this proc throttled */
    uint64_t              p_dispatchqueue_offset;
    uint64_t              p_dispatchqueue_serialno_offset;
    uint64_t              p_dispatchqueue_label_offset;
    uint64_t              p_return_to_kernel_offset;
    uint64_t              p_mach_thread_self_offset;
    pid_t                 p_responsible_pid;      /* pid resonsible for this process */
    _Atomic uint32_t      p_user_faults;          /* count the number of user faults generated */
    uint32_t              p_memlimit_increase;    /* byte increase for memory limit for dyld SPI rdar://problem/49950264, structure packing 32-bit and 64-bit */
    struct os_reason     *p_exit_reason;
    char *                p_subsystem_root_path;
    lck_rw_t             *p_dirs_lock;            /* keeps fd_cdir and fd_rdir stable across a lookup */
    pid_t                 p_sessionid;
};

struct pgrp {
    LIST_ENTRY(pgrp)      pg_hash;                /* Hash chain. (LL) */
    LIST_HEAD(, proc)     pg_members;             /* Pointer to pgrp members. (PGL) */
    struct session       *pg_session;             /* Pointer to session. (LL ) */
    pid_t                 pg_id;                  /* Pgrp id. (static) */
    int                   pg_jobc;                /* # procs qualifying pgrp for job control (PGL) */
    int                   pg_membercnt;           /* Number of processes in the pgrocess group (PGL) */
    int                   pg_refcount;            /* number of current iterators (LL) */
    unsigned int          pg_listflags;           /* (LL) */
    lck_mtx_t            *pg_mlock;               /* mutex lock to protect pgrp */
};

extern int nprocs, maxproc;                       /* Current and max number of procs. */

LIST_HEAD(proclist, proc);
extern struct proclist allproc;                   /* List of all processes. */

extern void proc_list_lock(void);
extern void proc_list_unlock(void);
extern void proc_fdlock(struct proc *);
extern void proc_fdlock_spin(struct proc *);
extern void proc_fdunlock(struct proc *);
extern void session_lock(struct session * sess);
extern void session_unlock(struct session * sess);

typedef int (*proc_iterate_fn_t)(proc_t, void *);

#define PROC_RETURNED     (0)

#define PROC_ALLPROCLIST  (1U << 0)               /* walk the allproc list (processes not yet exited) */

extern void proc_iterate(unsigned int flags, proc_iterate_fn_t callout, void *arg, proc_iterate_fn_t filterfn, void *filterarg);

// =================
// sys/uio_internal.h

struct uio {
	union iovecs         *uio_iovs;               /* current iovec */
	int                   uio_iovcnt;             /* active iovecs */
	off_t                 uio_offset;
	enum uio_seg          uio_segflg;
	enum uio_rw           uio_rw;
	user_size_t           uio_resid_64;
	int                   uio_size;               /* size for use with kfree */
	int                   uio_max_iovs;           /* max number of iovecs this uio_t can hold */
	u_int32_t             uio_flags;
};

// =================
// sys/user.h

struct vfs_context {
    thread_t        vc_thread;              /* pointer to Mach thread */
    kauth_cred_t    vc_ucred;               /* per thread credential */
};

// =================
// sys/vnode_internal.h

struct vnode {
	lck_mtx_t                 *v_lock;                        /* vnode mutex */
	TAILQ_ENTRY(vnode)         v_freelist;                    /* vnode freelist */
	TAILQ_ENTRY(vnode)         v_mntvnodes;                   /* vnodes for mount point */
	TAILQ_HEAD(, namecache)    v_ncchildren;                  /* name cache entries that regard us as their parent */
	LIST_HEAD(, namecache)     v_nclinks;                     /* name cache entries that name this vnode */
	vnode_t                    v_defer_reclaimlist;           /* in case we have to defer the reclaim to avoid recursion */
	uint32_t                   v_listflag;                    /* flags protected by the vnode_list_lock (see below) */
	uint32_t                   v_flag;                        /* vnode flags (see below) */
	uint16_t                   v_lflag;                       /* vnode local and named ref flags */
	uint8_t                    v_iterblkflags;                /* buf iterator flags */
	uint8_t                    v_references;                  /* number of times io_count has been granted */
	int32_t                    v_kusecount;                   /* count of in-kernel refs */
	int32_t                    v_usecount;                    /* reference count of users */
	int32_t                    v_iocount;                     /* iocounters */
	void *                     v_owner;                       /* act that owns the vnode */
	uint16_t                   v_type;                        /* vnode type */
	uint16_t                   v_tag;                         /* type of underlying data */
	uint32_t                   v_id;                          /* identity of vnode contents */
	union {
		struct mount          vu_mountedhere;                 /* ptr to mounted vfs (VDIR) */
		socket_t             *vu_socket;                      /* unix ipc (VSOCK) */
		struct specinfo      *vu_specinfo;                    /* device (VCHR, VBLK) */
		struct fifoinfo      *vu_fifoinfo;                    /* fifo (VFIFO) */
		struct ubc_info      *vu_ubcinfo;                     /* valid for (VREG) */
	} v_un;
	struct buflists          *v_cleanblkhd;                   /* clean blocklist head */
	struct buflists          *v_dirtyblkhd;                   /* dirty blocklist head */
	struct klist             *v_knotes;                       /* knotes attached to this vnode */
	kauth_cred_t              v_cred;                         /* last authorized credential */
	kauth_action_t            v_authorized_actions;           /* current authorized actions for v_cred */
	int                       v_cred_timestamp;               /* determine if entry is stale for MNTK_AUTH_OPAQUE */
	int                       v_nc_generation;                /* changes when nodes are removed from the name cache */
	int32_t                   v_numoutput;                    /* num of writes in progress */
	int32_t                   v_writecount;                   /* reference count of writers */
	const char               *v_name;                         /* name component of the vnode */
	vnode_t                   v_parent;                       /* pointer to parent vnode */
	struct lockf             *v_lockf;                        /* advisory lock list head */
	int(**v_op)(void *);                                      /* vnode operations vector */
	mount_t                   v_mount;                        /* ptr to vfs we are in */
	void *                    v_data;                         /* private data for fs */
};

#endif /* procfs_internal_h */
