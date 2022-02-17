/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_u.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 11:50:35 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/18 10:19:15 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static void		apply_specifier(t_format *data, uintmax_t *n)
{
	if (*data->tail == 'U')
		*n = (uintmax_t)(*n);
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

void			apply_half_flag(t_format *data,
		size_t nlen, size_t base_len, int f)
{
	if (f && msk(data, OPT_PREC) && data->flag.precision < 1)
		nlen = 0;
	if (!msk(data, OPT_MINUS))
		while (data->flag.width > nlen && data->flag.width--)
			write_char(data, (msk(data, OPT_ZERO)
						&& !msk(data, OPT_PREC) ? '0' : ' '));
	if (msk(data, OPT_HASH))
		write_char(data, '0');
	while (data->flag.precision > base_len && data->flag.precision--)
		write_char(data, '0');
}

void			apply_after_flag(t_format *data, size_t n_len)
{
	if (msk(data, OPT_MINUS) && data->flag.width > data->flag.precision)
		while (data->flag.width > n_len && data->flag.width--)
			write_char(data, ' ');
}

void			convert_u(t_format *data)
{
	char			*string;
	uintmax_t		n;
	size_t			n_len;
	size_t			base_len;

	n = 0;
	conv_star(data);
	string = NULL;
	get_arg_n(data, &n);
	apply_specifier(data, &n);
	base_len = ft_uintlen_base(n, 10);
	n_len = data->flag.precision > base_len ? data->flag.precision : base_len;
	apply_half_flag(data, n_len, base_len, n == 0 ? 1 : 0);
	if (msk(data, OPT_APS))
	{
		string = ft_convert_ubase(n, 10, n_len, 65);
		get_aps(data, string, n_len);
		free(string);
	}
	if (n == 0 && msk(data, OPT_PREC) && data->flag.precision < 1)
		;
	else
		write_nbr(data, 10, n, 65);
	apply_after_flag(data, n_len);
}
