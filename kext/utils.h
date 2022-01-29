#ifndef utils_h
#define utils_h

#include <mach/i386/kern_return.h>
#include <mach/i386/vm_types.h>
#include <mach/mach_types.h>
#include <mach-o/loader.h>
#include <sys/types.h>
#include <stdint.h>

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

/*
 * Created 181110  lynnl
 *
 * Help functions used in kernel extension
 */

/*
 * Used to indicate unused function parameters
 * see: <sys/cdefs.h>#__unused
 */
#define UNUSED(arg0, ...)   (void) ((void) arg0, ##__VA_ARGS__)

#define ARRAY_SIZE(x)       (sizeof(x) / sizeof(*(x)))

/*
 * literal-aware alias of extern modifier
 */
#define readonly_extern     extern

/*
 * G for GCC-specific
 * see:
 *  <sys/param.h>#MIN, MAX
 *  <libkern/libkern.h>#min, max
 */
#define GMIN(a, b) ({       \
    __typeof(a) _a = (a);   \
    __typeof(b) _b = (b);   \
    _a < _b ? _a : _b;      \
})

#define GMAX(a, b) ({       \
    __typeof(a) _a = (a);   \
    __typeof(b) _b = (b);   \
    _a > _b ? _a : _b;      \
})

#define panicf(fmt, ...)                \
    panic("\n" fmt "\n%s@%s#L%d\n\n",   \
            ##__VA_ARGS__, __BASE_FILE__, __FUNCTION__, __LINE__)

#ifdef DEBUG
/*
 * NOTE: Do NOT use any multi-nary conditional/logical operator inside assertion
 *       like operators && || ?:  it's extremely EVIL
 *       Separate them  each statement per line
 */
#define kassert(ex) (ex) ? (void) 0 : panicf("Assert `%s' failed", #ex)
/**
 * @ex      the expression
 * @fmt     panic message format
 *
 * Example: kassertf(sz > 0, "Why size %zd nonpositive?", sz);
 */
#define kassertf(ex, fmt, ...) \
    (ex) ? (void) 0 : panicf("Assert `%s' failed: " fmt, #ex, ##__VA_ARGS__)
#else
#define kassert(ex)             (void) ((void) (ex))
#define kassertf(ex, fmt, ...)  (void) ((void) (ex), ##__VA_ARGS__)
#endif

#define kassert_null(ptr)       kassert(((void *) ptr) == NULL)
#define kassert_nonnull(ptr)    kassert(((void *) ptr) != NULL)

/**
 * Branch predictions
 * see: linux/include/linux/compiler.h
 */
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

/**
 * os_log() is only available on macOS 10.12 or newer
 *  thus os_log do have compatibility issue  use printf instead
 *
 * XNU kernel version of printf() don't recognize some rarely used specifiers
 *  like h, i, j, t  use unrecognized spcifier may raise kernel panic
 *
 * Feel free to print NULL as %s  it checked explicitly by kernel-printf
 *
 * see: xnu/osfmk/kern/printf.c#printf
 */
#define LOG(fmt, ...)           printf(KEXTNAME_S ": " fmt "\n", ##__VA_ARGS__)

#define LOG_INF(fmt, ...)       LOG("[INF] " fmt, ##__VA_ARGS__)
#define LOG_WAR(fmt, ...)       LOG("[WAR] " fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)       LOG("[ERR] " fmt, ##__VA_ARGS__)
#define LOG_BUG(fmt, ...)       LOG("[BUG] " fmt, ##__VA_ARGS__)
#define LOG_OFF(fmt, ...)       (void) ((void) 0, ##__VA_ARGS__)
#ifdef DEBUG
#define LOG_DBG(fmt, ...)       LOG("[DBG %s#L%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...)       LOG_OFF(fmt, ##__VA_ARGS__)
#endif

#ifdef DEBUG
/**
 * Assert over a known flags
 * @param flags     the flags need to assert
 * @param known     the known flags
 * If met any unknown flags  it'll print a warning message instead of panic
 */
#define kassert_known_flags(flags, known) do {                              \
    uint64_t __a = (flags);                                                 \
    uint64_t __b = (known);                                                 \
    uint64_t __c = __a & ~__b;                                              \
    if (__c) {                                                              \
        LOG("[WAR %s#L%d] Met unknown flags %#llx (%#llx vs %#llx)",        \
                __func__, __LINE__, __c, __a, __b);                         \
    }                                                                       \
} while (0)
#else
#define kassert_known_flags(flags, known)   UNUSED(flags, known)
#endif

/*
 * Capital Q stands for quick
 * XXX: should only used for `char[]'  NEVER `char *'
 */
#define QSTRLEN(s)          (sizeof(s) - 1)

void *util_malloc(size_t, int);
void *util_realloc(void *, size_t, size_t, int);
void util_mfree(void *);
void util_massert(void);

int util_get_kcb(void);
int util_put_kcb(void);
int util_read_kcb(void);
void util_invalidate_kcb(void);

int util_vma_uuid(const vm_address_t addr, uuid_string_t output);

void format_uuid_string(const uuid_t, uuid_string_t);

/**
 * kern_os_* family provides zero-out memory allocation
 * see: xnu/libkern/c++/OSRuntime.cpp
 */
extern void *kern_os_malloc(size_t);
extern void kern_os_free(void *);
extern void *kern_os_realloc(void *, size_t);

#endif /* utils_h */
