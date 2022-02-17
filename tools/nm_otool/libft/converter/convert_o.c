/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_o.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 11:50:53 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/18 10:18:47 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static void			apply_specifier(t_format *data, uintmax_t *n)
{
	if (*data->tail == 'O')
		*n = (unsigned long long)(*n);
	else if (data->flag.size_modifier & OPT_H)
		*n = (unsigned short)(*n);
	else if (data->flag.size_modifier & OPT_HH)
		*n = (unsigned char)(*n);
	else if (data->flag.size_modifier & OPT_LL)
		*n = (unsigned long long)(*n);
	else if (data->flag.size_modifier & OPT_L)
		*n = (unsigned long)(*n);
	else if (data->flag.size_modifier & OPT_J)
		*n = (uintmax_t)(*n);
	else if (data->flag.size_modifier & OPT_Z)
		*n = (size_t)(*n);
	else
		*n = (unsigned int)(*n);
}

static void			apply_half_flag(t_format *data, size_t n_len, int f)
{
	size_t			save;

	save = n_len;
	if (msk(data, OPT_HASH) && f)
		save += 1;
	if (data->flag.precision > save)
		save = data->flag.precision;
	if (!f && msk(data, OPT_PREC) && data->flag.precision < 1)
		save = 0;
	if (!msk(data, OPT_MINUS))
	{
		while (data->flag.width > save && data->flag.width--)
			write_char(data, msk(data, OPT_ZERO)
						&& (!msk(data, OPT_PREC)) ? '0' : ' ');
	}
	if ((msk(data, OPT_HASH) && f) || (!f && msk(data, OPT_HASH)
				&& msk(data, OPT_PREC) && data->flag.precision < 1))
		write_char(data, '0');
}

void				convert_o(t_format *data)
{
	uintmax_t		n;
	size_t			n_len;
	size_t			precision;
	size_t			save;

	n = 0;
	save = 0;
	conv_star(data);
	get_arg_n(data, &n);
	apply_specifier(data, &n);
	n_len = ft_uintlen_base(n, 8);
	apply_half_flag(data, n_len, n > 0 ? 1 : 0);
	precision = data->flag.precision;
	save = (msk(data, OPT_HASH) && n) ? n_len + 1 : n_len;
	while (precision > save && precision--)
		write_char(data, '0');
	if (!(n == 0 && msk(data, OPT_PREC) && data->flag.precision < 1))
		write_nbr(data, 8, n, 0);
	if (data->flag.opts & OPT_MINUS)
	{
		if (data->flag.precision > save)
			save = data->flag.precision;
		while (data->flag.width > save && data->flag.width--)
			write_char(data, ' ');
	}
}
