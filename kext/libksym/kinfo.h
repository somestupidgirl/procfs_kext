#ifndef _kinfo_h
#define _kinfo_h

#define KERN_ADDR_MASK                  0xfffffffffff00000u
#define KERN_ADDR_STEP                  (~KERN_ADDR_MASK + 1u)

#define ADDR_BUFSZ                      19

#define KERN_HIB_BASE2                   ((vm_offset_t) get_hib_base() + 0ULL)
#define KERN_TEXT_BASE2                  ((vm_offset_t) get_kern_base() + 0ULL)

extern int get_magic(void);
extern int get_filetype(void);
extern vm_offset_t get_kern_base(void);
extern vm_offset_t get_hib_base(void);

#endif /* _kinfo_h */