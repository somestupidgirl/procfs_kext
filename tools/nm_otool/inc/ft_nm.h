/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_nm.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/11/14 13:52:31 by dbaffier          #+#    #+#             */
/*   Updated: 2020/08/23 17:39:12 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FT_NM_H
# define FT_NM_H

# include <stdbool.h>
# include "ft_ofile.h"

# define ARGS0 "	-a\tDisplay all symbol table entries.\n"
# define ARGS1 "	-g\tDisplay only global (external) symbols.\n"
# define ARGS3 "	-o\tPrepend file or archive element name to each line.\n"
# define ARGS4 "	-p\tDon't sort; display in symbol-table order.\n"
# define ARGS6 "	-u\tDisplay only undefined symbols.\n"
# define ARGS7 "	-U\tDon't display undefined symbols. doesn't work btw\n"
# define ARGS8 "	-j\tJust display the symbol names (no value or type).\n"
# define ARGS9 "	-A\tWrite the pathname or library name on each line.\n"

/*
** Flag structure padded to 1, given to processor function as a (void *)cookie
** a - Display all symbol table entries, including those
**     inserted for use by debuggers.
** g - Display only global (external) symbols.
** o - Prepend file or archive element name to each output line, rather
**     than only once.
** p - Don't sort; display in symbol-table order.
** u - Display only undefined symbols.
** uu(U) - Don't display undefined symbols. Doesn't work
** j - Just display the symbol names (no value or type).
** aa(A) - Write the pathname or library name of an object on each line.
*/

typedef struct				s_flags
{
	unsigned int a : 1;
	unsigned int g : 1;
	unsigned int o : 1;
	unsigned int p : 1;
	unsigned int u : 1;
	unsigned int uu : 1;
	unsigned int j : 1;
	unsigned int aa : 1;
}							t_flags;

/*
** Structure to store symbol
** *name of symbol
** if type of symbol is N_IDNR - The symbol
** is defined to be the same as another symbol
** nlist_64 - Describe an entry in the symbol table
*/

struct						s_symbol
{
	char					*name;
	char					*indr_name;
	struct nlist_64			nl;
};

/*
** nsects - Indicates the number of section
** data structures following this load command.
** text_nsect - data_nsect - bss_nsect - For printing symbols types, T, D, and B
** section - array of symbol from the target in 32 or 64
*/

typedef struct				s_process_flg
{
	uint32_t				nsects;
	unsigned char			text_nsect;
	unsigned char			data_nsect;
	unsigned char			bss_nsect;
	struct section			**sections;
	struct section_64		**sections64;
}							t_process_flg;

/*
** See stab.c
*/

struct						s_stabnames
{
	unsigned char			n_type;
	char					*name;
};

/*
** global structure for nm processor.
*/

typedef struct				s_nm
{
	uint32_t				ncmds;
	uint32_t				mh_flags;
	uint32_t				nsymbs;
	struct symtab_command	*st;
	struct s_symbol			*select_sym;
	struct s_symbol			*sym;
	struct nlist			*nl;
	struct nlist_64			*nl64;
	t_process_flg			*flg;
}							t_nm;

int							print_stab(t_nm *nm, size_t i);
int							parse_flag(t_flags *f, char **av, int ac, int i);
void						make_symbol_64(struct s_symbol *symbol,
					struct nlist_64 *nl);
void						make_symbol_32(struct s_symbol *symbol,
			struct nlist *nl);
int							select_symbol(struct s_symbol *symbol, t_flags *f);
void						select_print_symbols(t_nm *nm, t_ofile *of,
			int size);
int							type_symbol(t_process_flg *f, struct s_symbol sl);
void						print_header(t_ofile *of, t_flags *f);
void						print_symbols(t_ofile *of, t_nm *nm, t_flags *f);
void						process_flg_sect(t_nm *nm, t_ofile *of,
		t_process_flg *f, struct load_command *lc);

#endif
