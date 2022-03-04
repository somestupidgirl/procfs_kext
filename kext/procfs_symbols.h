#ifndef _procfs_symbols_h
#define _procfs_symbols_h

#include <libklookup/klookup.h>

#include "procfs_internal.h"

extern kern_return_t resolve_symbols(void);

#define SYM_INIT(sym) __typeof(_##sym) _##sym = NULL

#define SYM_LOOKUP(sym) { \
    if (!(_##sym = SymbolLookup("_"#sym))) { \
        return KERN_FAILURE; \
    } \
}

#endif /* _procfs_symbols_h */
