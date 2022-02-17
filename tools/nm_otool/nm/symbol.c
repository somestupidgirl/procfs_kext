/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   symbol.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/11/16 16:31:53 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:03:32 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_nm.h"
#include "ft_ofile.h"

void	make_symbol_32(struct s_symbol *symbol, struct nlist *nl)
{
	symbol->nl.n_un.n_strx = nl->n_un.n_strx;
	symbol->nl.n_type = nl->n_type;
	symbol->nl.n_sect = nl->n_sect;
	symbol->nl.n_desc = nl->n_desc;
	symbol->nl.n_value = nl->n_value;
}

void	make_symbol_64(struct s_symbol *symbol, struct nlist_64 *nl)
{
	symbol->nl = *nl;
}

int		select_symbol(struct s_symbol *symbol, t_flags *f)
{
	if (f->uu)
	{
		if ((symbol->nl.n_type == (N_UNDF | N_EXT) &&
			symbol->nl.n_value == 0) ||
			symbol->nl.n_type == (N_PBUD | N_EXT))
			return (0);
		return (1);
	}
	if (f->u)
	{
		if ((symbol->nl.n_type == (N_UNDF | N_EXT) &&
			symbol->nl.n_value == 0) ||
			symbol->nl.n_type == (N_PBUD | N_EXT))
			return (1);
		return (0);
	}
	if (f->g && (symbol->nl.n_type & N_EXT) == 0)
		return (0);
	if (symbol->nl.n_type & N_STAB &&
		(f->a == 0 || f->g == 1 || f->u == 1))
		return (0);
	return (1);
}

void	select_print_symbols(t_nm *nm, t_ofile *of, int size)
{
	char	*str;
	size_t	i;

	i = 0;
	(void)size;
	str = of->object_addr + nm->st->stroff;
	while (i < nm->nsymbs)
	{
		if (nm->select_sym[i].nl.n_un.n_strx == 0)
			nm->select_sym[i].name = "";
		else
			nm->select_sym[i].name = nm->select_sym[i].nl.n_un.n_strx + str;
		if ((nm->select_sym[i].nl.n_type & N_STAB) == 0 &&
			(nm->select_sym[i].nl.n_type & N_TYPE) == N_INDR)
		{
			if (nm->select_sym[i].nl.n_value == 0)
				nm->select_sym[i].indr_name = "";
			else
				nm->select_sym[i].indr_name =
					str + nm->select_sym[i].nl.n_value;
		}
		i++;
	}
}
