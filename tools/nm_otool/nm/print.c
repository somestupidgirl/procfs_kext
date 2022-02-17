/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   print.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/11/16 23:35:23 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/07 20:46:52 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_nm.h"

void		print_header(t_ofile *of, t_flags *f)
{
	if (of->member_ar_hdr && (f->o == 0 && f->aa == 0))
	{
		if (of->member_ar_hdr != NULL)
			ft_printf("\n%s(%.*s):\n", of->file_name,
				(int)of->member_name_size, of->member_name);
		else
			ft_printf("\n%s:\n", of->file_name);
	}
}

static void	quicksort(struct s_symbol *symbol, int size)
{
	struct s_symbol	tmp;
	int				i;

	i = 0;
	while (i < size)
	{
		if (i + 1 < size && ft_strcmp(symbol[i + 1].name, symbol[i].name) < 0)
		{
			tmp = symbol[i + 1];
			symbol[i + 1] = symbol[i];
			symbol[i] = tmp;
			i = 0;
		}
		else
			i++;
	}
}

void		print_classic(t_nm *nm, char c, size_t i)
{
	if (nm->select_sym[i].nl.n_type & N_EXT && c != '?')
		c = ft_toupper(c);
	if (c == 'u' || c == 'U' || c == 'i' || c == 'I')
		ft_printf("%*s ", 16, "");
	else
		ft_printf("%016llx ", nm->select_sym[i].nl.n_value);
	ft_printf("%c ", c);
}

/*
** Print symbol stored in select_sym array accordingly to flag
*/

void		print_symbols(t_ofile *of, t_nm *nm, t_flags *f)
{
	char	c;
	size_t	i;

	i = -1;
	if (f->p == 0)
		quicksort(nm->select_sym, nm->nsymbs);
	while (++i < nm->nsymbs)
	{
		if (print_stab(nm, i))
			continue;
		c = type_symbol(nm->flg, nm->select_sym[i]);
		if (f->u && c != 'u')
			continue;
		if (f->o || f->aa)
		{
			if (of->member_ar_hdr)
				ft_printf("%s:%.*s: ", of->file_name,
				(int)of->member_name_size, of->member_name);
			else
				ft_printf("%s: ", of->file_name);
		}
		if (f->u == 0 && f->j == 0)
			print_classic(nm, c, i);
		ft_printf("%s\n", nm->select_sym[i].name);
	}
}
