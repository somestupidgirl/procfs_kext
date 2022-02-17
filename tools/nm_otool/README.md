# nm_otool
Re-writing the two system tools nm and otool for MacOS-X and Linux

#### FT_NM

**ft_nm** displays  the name list (symbol table) of each object file in the argument list.

If an argument is an archive, a listing for each object file in the archive will be produced.

If no file is given, the symbols in a.out are listed.

    For each symbol, ft_nm shows:
    
       ·   The symbol value, in the radix selected by options, or hexadecimal by default.

       ·   The symbol type.  At least the following types are used; others are, as well, depending on
           the object file format.  If lowercase, the symbol is usually local; if uppercase, the
           symbol is global (external).  There are however a few lowercase symbols that are shown for
           special global symbols ("u", "v" and "w").

       ·   The symbol name.
       
#### USAGE

    $> ft_nm  [ -agnopruUjA ] [ file ... ]
        -a    Display all symbol table entries.
        -g    Display only global (external) symbols.
        -n    Sort numerically rather than alphabetically.
        -o    Prepend file or archive element name to each line.
        -p    Don't sort; display in symbol-table order.
        -r    Sort in reverse order.
        -u    Display only undefined symbols.
        -U    Don't display undefined symbols.
        -j    Just display the symbol names (no value or type).
        -A    Write the pathname or library name on each line.

## FT_OTOOL

**ft_otool** displays specified parts of object files or libraries.

At least one of the following options must be specified.

#### USAGE

    $> ft_otool [ -fahLtd ] [file ... ]
        -f    Display the Fat headers.
        -a    Display the archive header.
        -h    Display the Mach header.
        -L    Display the shared libraries used.
        -t    Display the contents of the (__TEXT,__text) section.
        -d    Display the contents of the (__DATA,__data) section.
