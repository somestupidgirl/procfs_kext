//
//  resolver.h
//  test
//
//  Created by 五千年木 on 2018/12/24.
//  Copyright © 2018年 osxkernel. All rights reserved.
//

#ifndef resolver_h
#define resolver_h

#include <mach/mach_types.h>
#include <mach-o/loader.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>

#include <libkern/libkern.h>
#include <libkern/version.h>
#include <libkern/OSMalloc.h>

/**
 * \brief   Find kernel segment with name
 */
struct segment_command_64* find_segment_64(const struct mach_header_64* mh, const char* segname);

/**
 * \brief   Resolve private kernel symbol for loaded kernel image
 */
void* resolve_kernel_symbol(const char* name, uintptr_t loaded_kernel_base);


/*
 * Each underlying filesystem allocates its own private area and hangs
 * it from v_data.  If non-null, this area is free in getnewvnode().
 */
//struct moon_vnode {
//    u_long	v_flag;				/* vnode flags (see below) */
//    short	v_usecount;			/* reference count of users */
//    short	v_writecount;			/* reference count of writers */
//    long	v_holdcnt;			/* page & buffer references */
//    daddr_t	v_lastr;			/* last read (read-ahead) */
//    u_long	v_id;				/* capability identifier */
//    struct	mount *v_mount;			/* ptr to vfs we are in */
//    int 	(**v_op)();			/* vnode operations vector */
//    struct	vnode *v_freef;			/* vnode freelist forward */
//    struct	vnode **v_freeb;		/* vnode freelist back */
//    struct	vnode *v_mountf;		/* vnode mountlist forward */
//    struct	vnode **v_mountb;		/* vnode mountlist back */
//    struct	list_entry v_cleanblkhd;	/* clean blocklist head */
//    struct	list_entry v_dirtyblkhd;	/* dirty blocklist head */
//    long	v_numoutput;			/* num of writes in progress */
//    enum	vtype v_type;			/* vnode type */
//    union {
//        struct mount	*vu_mountedhere;/* ptr to mounted vfs (VDIR) */
//        struct socket	*vu_socket;	/* unix ipc (VSOCK) */
//        caddr_t		vu_vmdata;	/* private data for vm (VREG) */
//        struct specinfo	*vu_specinfo;	/* device (VCHR, VBLK) */
//        struct fifoinfo	*vu_fifoinfo;	/* fifo (VFIFO) */
//    } v_un;
//    struct	nqlease *v_lease;		/* Soft reference to lease */
//    daddr_t	v_lastw;			/* last write (write cluster) */
//    daddr_t	v_cstart;			/* start block of cluster */
//    daddr_t	v_lasta;			/* last allocation */
//    int	v_clen;				/* length of current cluster */
//    int	v_ralen;			/* Read-ahead length */
//    long	v_spare[8];			/* round to 128 bytes */
//    enum	vtagtype v_tag;			/* type of underlying data */
//    void 	*v_data;			/* private data for fs */
//};

//struct vnode_attr {
//    /* bitfields */
//    uint64_t	va_supported;
//    uint64_t	va_active;
//    
//    /*
//     * Control flags.  The low 16 bits are reserved for the
//     * ioflags being passed for truncation operations.
//     */
//    int		va_vaflags;

//    /* traditional stat(2) parameter fields */
//    dev_t		va_rdev;	/* device id (device nodes only) */
//    uint64_t	va_nlink;	/* number of references to this file */
//    uint64_t	va_total_size;	/* size in bytes of all forks */
//    uint64_t	va_total_alloc;	/* disk space used by all forks */
//    uint64_t	va_data_size;	/* size in bytes of the fork managed by current vnode */
//    uint64_t	va_data_alloc;	/* disk space used by the fork managed by current vnode */
//    uint32_t	va_iosize;	/* optimal I/O blocksize */
//    
//    /* file security information */
//    uid_t		va_uid;		/* owner UID */
//    gid_t		va_gid;		/* owner GID */
//    mode_t		va_mode;	/* posix permissions */
//    uint32_t	va_flags;	/* file flags */
//    struct kauth_acl *va_acl;	/* access control list */
//    
//    /* timestamps */
//    struct timespec	va_create_time;	/* time of creation */
//    struct timespec	va_access_time;	/* time of last access */
//    struct timespec	va_modify_time;	/* time of last data modification */
//    struct timespec	va_change_time;	/* time of last metadata change */
//    struct timespec	va_backup_time;	/* time of last backup */
//    
//    /* file parameters */
//    uint64_t	va_fileid;	/* file unique ID in filesystem */
//    uint64_t	va_linkid;	/* file link unique ID */
//    uint64_t	va_parentid;	/* parent ID */
//    uint32_t	va_fsid;	/* filesystem ID */
//    uint64_t	va_filerev;	/* file revision counter */	/* XXX */
//    uint32_t	va_gen;		/* file generation count */	/* XXX - relationship of
//                                                         * these two? */
//    /* misc parameters */
//    uint32_t	va_encoding;	/* filename encoding script */
//    
//    enum vtype	va_type;	/* file type */
//    char *		va_name;	/* Name for ATTR_CMN_NAME; MAXPATHLEN bytes */
//    guid_t		va_uuuid;	/* file owner UUID */
//    guid_t		va_guuid;	/* file group UUID */
//    
//    /* Meaningful for directories only */
//    uint64_t	va_nchildren;     /* Number of items in a directory */
//    uint64_t	va_dirlinkcount;  /* Real references to dir (i.e. excluding "." and ".." refs) */
//    
//    void * 		va_reserved1;
//    struct timespec va_addedtime;	/* timestamp when item was added to parent directory */
//    
//    /* Data Protection fields */
//    uint32_t va_dataprotect_class;	/* class specified for this file if it didn't exist */
//    uint32_t va_dataprotect_flags;	/* flags from NP open(2) to the filesystem */
//    
//    /* Document revision tracking */
//    uint32_t va_document_id;
//    
//    /* Fields for Bulk args */
//    uint32_t 	va_devid;	/* devid of filesystem */
//    uint32_t 	va_objtype;	/* type of object */
//    uint32_t 	va_objtag;	/* vnode tag of filesystem */
//    uint32_t 	va_user_access;	/* access for user */
//    uint8_t  	va_finderinfo[32];	/* Finder Info */
//    uint64_t 	va_rsrc_length;	/* Resource Fork length */
//    uint64_t 	va_rsrc_alloc;	/* Resource Fork allocation size */
//    fsid_t 		va_fsid64;	/* fsid, of the correct type  */
//    
//    uint32_t va_write_gencount;     /* counter that increments each time the file changes */
//    
//    uint64_t va_private_size; /* If the file were deleted, how many bytes would be freed immediately */
//    
//    /* add new fields here only */
//};

#endif /* resolver_h */
