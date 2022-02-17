/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_p.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 11:49:29 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/18 10:18:56 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static void		apply_half_flag(t_format *data, size_t n_len)
{
	size_t		save;

	save = n_len;
	if (!msk(data, OPT_MINUS))
	{
		if (data->flag.width <= n_len + 2 || msk(data, OPT_ZERO))
			write_str(data, "0x");
		while (data->flag.width > (n_len + 2) && n_len++)
			write_char(data, msk(data, OPT_ZERO) ? '0' : ' ');
		n_len = data->flag.precision > save ? data->flag.precision : save;
		if (data->flag.width > n_len + 2 && (!msk(data, OPT_ZERO)))
			write_str(data, "0x");
	}
	else
		write_str(data, "0x");
}

void			convert_p(t_format *data)
{
	uintmax_t			n;
	size_t				n_len;

	conv_star(data);
	get_arg_n(data, &n);
	n_len = ft_uintlen_base(n, 16);
	apply_half_flag(data, n_len);
	while (data->flag.precision > n_len && data->flag.precision--)
		write_char(data, '0');
	if (n || (msk(data, OPT_PREC) && data->flag.precision > 0)
			|| !msk(data, OPT_PREC))
		write_nbr(data, 16, n, 97);
	if (msk(data, OPT_MINUS))
		while (data->flag.width > (n_len + 2) && data->flag.width--)
			write_char(data, ' ');
}
