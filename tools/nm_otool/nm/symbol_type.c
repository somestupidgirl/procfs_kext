/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   symbol_type.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/11/19 16:16:29 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/07 20:47:02 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_nm.h"

static int	sect_more(t_process_flg *f, int n_type)
{
	if (n_type == f->text_nsect)
		return ('t');
	else if (n_type == f->data_nsect)
		return ('d');
	else if (n_type == f->bss_nsect)
		return ('b');
	return ('s');
}

static int	symbol_n_type(t_process_flg *f, char c, int nl_value, int nl_sect)
{
	if (c == N_UNDF)
		return (nl_value != 0 ? 'c' : 'u');
	else if (c == N_PBUD)
		return ('u');
	else if (c == N_ABS)
		return ('a');
	else if (c == N_SECT)
		return (sect_more(f, nl_sect));
	else if (c == N_INDR)
		return ('i');
	return ('?');
}

int			type_symbol(t_process_flg *f, struct s_symbol sl)
{
	char	c;

	c = symbol_n_type(f, sl.nl.n_type & N_TYPE, sl.nl.n_value, sl.nl.n_sect);
	return (c);
}
