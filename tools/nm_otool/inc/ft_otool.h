/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_otool.h                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/08 21:44:32 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:13:09 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FT_OTOOL_H
# define FT_OTOOL_H

# include "ft_ofile.h"
# include "libft.h"

/*
** f - option to display the universal headers is -universal-headers.
** l - doesn't work
** h - option to display the  Mach  header  is  -private-header.
** ll(L) - doesn't work
** dd(D) - option  to  display  just  the install name of
** a shared library is -dylib-id.
** t - option   to   display   the  contents  of  the (__TEXT,__text) section
** d - doesn't work
** ss(S) - obsolète, doesn't work anyway
** object_process boolean to determine if we
**  should process current object or not based on flag.
*/

typedef struct		s_flags
{
	unsigned int	f : 1;
	unsigned int	l : 1;
	unsigned int	h : 1;
	unsigned int	ll : 1;
	unsigned int	dd : 1;
	unsigned int	t : 1;
	unsigned int	d : 1;
	unsigned int	ss : 1;
	int				object_process : 1;
}					t_flags;

/*
** Global structure for processor, Otool.
*/

typedef struct		s_data
{
	uint32_t					cmd;
	int							found;
	char						*addr;
	unsigned long				size;
	cpu_type_t					mh_cputype;
	cpu_subtype_t				mh_cpusubtype;
	uint32_t					mh_magic;
	uint32_t					mh_filetype;
	uint32_t					mh_ncmds;
	uint32_t					mh_sizeofcmds;
	uint32_t					sizeof_mach_header;
	char						*sect;
	uint32_t					sect_nrelocs;
	uint32_t					sect_flags;
	uint64_t					sect_addr;
	uint64_t					sect_size;
	uint64_t					seg_addr;
	struct relocation_info		*sect_relocs;
	struct section_64			*s64;
	struct segment_command_64	*sg64;
}					t_data;

int					parse_flag(t_flags *f, char **av, int ac, int i);

void				processor(t_ofile *of, char *arch_name, void *cookie);

void				print_header(t_ofile *of);
void				print_loadcmds(t_ofile *of, t_data *data);
void				print_mach_header(struct mach_header *mh);
void				print_mach_header_64(struct mach_header_64 *mh64);
void				print_libraries(struct load_command *load_c, t_data *data);
void				print_lc_symtab(t_ofile *of,
			t_data *data, struct load_command *lc);
void				print_lc_segment32(t_ofile *of,
			t_data *data, struct load_command *lc);
void				print_lc_segment64(t_ofile *of,
			t_data *data, struct load_command *lc);
void				get_sect_info(t_ofile *of, t_data *data);
void				print_text(t_data *data);

#endif
