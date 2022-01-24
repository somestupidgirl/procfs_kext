#ifndef utils_h
#define utils_h

#include <mach/i386/kern_return.h>
#include <mach/i386/vm_types.h>
#include <mach/mach_types.h>
#include <sys/types.h>
#include <stdint.h>

struct mach_header_64 {
    uint32_t    magic;      /* mach magic number identifier */
    cpu_type_t  cputype;    /* cpu specifier */
    cpu_subtype_t   cpusubtype; /* machine specifier */
    uint32_t    filetype;   /* type of file */
    uint32_t    ncmds;      /* number of load commands */
    uint32_t    sizeofcmds; /* the size of all the load commands */
    uint32_t    flags;      /* flags */
    uint32_t    reserved;   /* reserved */
};

struct kernel_info
{
    mach_vm_address_t       running_text_addr;      // the address of running __TEXT segment
    mach_vm_address_t       disk_text_addr;         // the same address at /mach_kernel in filesystem
    mach_vm_address_t       kaslr_slide;            // the kernel aslr slide, computed as the difference between above's addresses
    void                   *linkedit_buf;           // pointer to __LINKEDIT buffer containing symbols to solve
    uint64_t                linkedit_fileoff;       // __LINKEDIT file offset so we can read
    uint64_t                linkedit_size;
    uint32_t                symboltable_fileoff;    // file offset to symbol table - used to position inside the __LINKEDIT buffer
    uint32_t                symboltable_nr_symbols;
    uint32_t                stringtable_fileoff;    // file offset to string table
    uint32_t                stringtable_size;
    // other info from the header we might need
    uint64_t                text_size;              // size of __text section to disassemble
    struct                  mach_header_64 *mh;     // ptr to mach-o header of running kernel
};

struct symtab_command {
    uint32_t    cmd;        /* LC_SYMTAB */
    uint32_t    cmdsize;    /* sizeof(struct symtab_command) */
    uint32_t    symoff;     /* symbol table offset */
    uint32_t    nsyms;      /* number of symbol table entries */
    uint32_t    stroff;     /* string table offset */
    uint32_t    strsize;    /* string table size in bytes */
};

struct load_command {
    uint32_t cmd;       /* type of load command */
    uint32_t cmdsize;   /* total size of command in bytes */
};

struct segment_command_64 { /* for 64-bit architectures */
    uint32_t    cmd;        /* LC_SEGMENT_64 */
    uint32_t    cmdsize;    /* includes sizeof section_64 structs */
    char        segname[16];    /* segment name */
    uint64_t    vmaddr;     /* memory address of this segment */
    uint64_t    vmsize;     /* memory size of this segment */
    uint64_t    fileoff;    /* file offset of this segment */
    uint64_t    filesize;   /* amount to map from the file */
    vm_prot_t   maxprot;    /* maximum VM protection */
    vm_prot_t   initprot;   /* initial VM protection */
    uint32_t    nsects;     /* number of sections in segment */
    uint32_t    flags;      /* flags */
};

struct section_64 { /* for 64-bit architectures */
    char        sectname[16];   /* name of this section */
    char        segname[16];    /* segment this section goes in */
    uint64_t    addr;       /* memory address of this section */
    uint64_t    size;       /* size in bytes of this section */
    uint32_t    offset;     /* file offset of this section */
    uint32_t    align;      /* section alignment (power of 2) */
    uint32_t    reloff;     /* file offset of relocation entries */
    uint32_t    nreloc;     /* number of relocation entries */
    uint32_t    flags;      /* flags (section type and attributes)*/
    uint32_t    reserved1;  /* reserved (for offset or index) */
    uint32_t    reserved2;  /* reserved (for count or sizeof) */
    uint32_t    reserved3;  /* reserved */
};

struct descriptor_idt
{
    uint16_t offset_low;
    uint16_t seg_selector;
    uint8_t reserved;
    uint8_t flag;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved2;
};

#define MACH_KERNEL         "/System/Library/Kernels/kernel"
#define MH_MAGIC_64         0xfeedfacf
#define MH_EXECUTE          0x2
#define LC_SEGMENT_64       0x19
#define LC_SYMTAB           0x2

kern_return_t init_kernel_info(struct kernel_info *kinfo);
kern_return_t cleanup_kernel_info(struct kernel_info *kinfo);
mach_vm_address_t solve_kernel_symbol(struct kernel_info *kinfo, char *symbol_to_solve);
void get_addr_idt(mach_vm_address_t *idt);
uint16_t get_size_idt(void);

#define log(fmt, ...)           printf(PROCFS_NAME ": " fmt "\n", ##__VA_ARGS__)
#ifdef DEBUG
#define log_debug(fmt, ...)     printf(PROCFS_NAME ": " fmt " (%s:%d)\n", ##__VA_ARGS__, __func__, __LINE__)
#else
#define log_debug(fmt, ...)     ((void) 0)
#endif

#endif /* utils_h */
