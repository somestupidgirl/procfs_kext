/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_x.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 11:49:54 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/18 10:19:22 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static void		apply_half_flag(t_format *data, size_t n_len, int n)
{
	int				cond;

	cond = msk(data, OPT_ZERO) && !msk(data, OPT_PREC);
	if (msk(data, OPT_HASH) && n && msk(data, OPT_ZERO) && cond)
		write_str(data, "0x");
	if (!msk(data, OPT_MINUS))
	{
		while (data->flag.width > n_len && data->flag.width--)
			write_char(data, cond ? '0' : ' ');
	}
	if (msk(data, OPT_HASH) && n && !cond)
		write_str(data, "0x");
}

static void		apply_specifier(t_format *data, uintmax_t *n)
{
	if (data->flag.size_modifier & OPT_H)
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

void			convert_x(t_format *data)
{
	uintmax_t		n;
	size_t			n_len;
	size_t			save;

	n = 0;
	conv_star(data);
	get_arg_n(data, &n);
	apply_specifier(data, &n);
	n_len = ft_uintlen_base(n, 16);
	if (n == 0 && msk(data, OPT_PREC) && data->flag.precision <= 0)
		n_len = 0;
	save = n_len;
	n_len = data->flag.precision > n_len ? data->flag.precision : n_len;
	if (msk(data, OPT_HASH) && n)
		n_len += 2;
	apply_half_flag(data, n_len, n == 0 ? 0 : 1);
	while (data->flag.precision > save && data->flag.precision--)
		write_char(data, '0');
	if (n == 0 && msk(data, OPT_PREC) && data->flag.precision <= 0)
		;
	else
		write_nbr(data, 16, n, 97);
	if (msk(data, OPT_MINUS))
		while (data->flag.width > n_len && data->flag.width--)
			write_char(data, ' ');
}
