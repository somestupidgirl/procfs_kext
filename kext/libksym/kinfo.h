#ifndef _kinfo_h
#define _kinfo_h

#define KERN_ADDR_MASK 				0xfffffffffff00000u
#define KERN_ADDR_STEP 				(~KERN_ADDR_MASK + 1u)

#define ADDR_BUFSZ          		19

extern int get_magic(void);
extern int get_filetype(void);
extern int get_kern_base(void);
extern int get_hib_base(void);

#endif /* _kinfo_h */