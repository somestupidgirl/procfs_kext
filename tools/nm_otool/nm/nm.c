/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   nm.c                                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/11/10 23:50:02 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:53:38 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"
#include "ft_nm.h"

/*
** Allocate array of symbol and store it inside dependings on the flag
*/

void		select_symbols(t_nm *nm, t_ofile *of, t_flags *f)
{
	struct s_symbol		symbol;
	struct nlist_64		*nl64;
	uint32_t			nsymbols;
	uint32_t			i;

	i = 0;
	nl64 = NULL;
	nsymbols = 0;
	if (of->mh != NULL)
		nm->nl = (struct nlist *)(of->object_addr + nm->st->symoff);
	else
		nm->nl64 = (struct nlist_64 *)(of->object_addr + nm->st->symoff);
	if (!(nm->select_sym = malloc(sizeof(struct s_symbol) * nm->st->nsyms)))
		return ;
	while (i < nm->st->nsyms)
	{
		(of->mh != NULL) ? make_symbol_32(&symbol, nm->nl + i)
		: make_symbol_64(&symbol, nm->nl64 + i);
		if (select_symbol(&symbol, f))
			nm->select_sym[nsymbols++] = symbol;
		i++;
	}
	nm->nsymbs = nsymbols;
}

/*
** Store every struct and process it.
*/

static int	nm_set(t_nm *nm, t_ofile *ofile, t_process_flg *f, void *cookie)
{
	struct load_command		*lc;
	size_t					i;

	i = 0;
	nm->ncmds = (ofile->mh) ? ofile->mh->ncmds : ofile->mh64->ncmds;
	nm->mh_flags = (ofile->mh) ? ofile->mh->flags : ofile->mh64->flags;
	lc = ofile->load_commands;
	while (i < nm->ncmds)
	{
		if (nm->st == NULL && lc->cmd == LC_SYMTAB)
			nm->st = (struct symtab_command *)lc;
		else if (lc->cmd == LC_SEGMENT)
			f->nsects += ((struct segment_command *)lc)->nsects;
		else if (lc->cmd == LC_SEGMENT_64)
			f->nsects += ((struct segment_command_64 *)lc)->nsects;
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
		i++;
	}
	if (nm->st == NULL || nm->st->nsyms == 0)
		return (1);
	process_flg_sect(nm, ofile, f, ofile->load_commands);
	select_symbols(nm, ofile, cookie);
	select_print_symbols(nm, ofile, nm->st->strsize);
	return (0);
}

/*
** Processor NM.
** Print data in ofile struct.
** Apply is given flag by (void *)cookie.
*/

void		nm(t_ofile *ofile, char *arch_name, void *cookie)
{
	t_nm			nm;
	t_process_flg	process_flag;

	(void)arch_name;
	if (ofile->mh == NULL && ofile->mh64 == NULL)
		return ;
	ft_memset(&nm, 0, sizeof(t_nm));
	ft_memset(&process_flag, 0, sizeof(process_flag));
	process_flag.text_nsect = NO_SECT;
	process_flag.data_nsect = NO_SECT;
	process_flag.bss_nsect = NO_SECT;
	nm.flg = &process_flag;
	nm_set(&nm, ofile, &process_flag, cookie);
	print_header(ofile, cookie);
	print_symbols(ofile, &nm, cookie);
	free(nm.select_sym);
	process_flag.sections ? free(process_flag.sections)
						: free(process_flag.sections64);
}
