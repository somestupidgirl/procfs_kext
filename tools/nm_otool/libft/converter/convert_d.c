/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_d.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 11:51:12 by dbaffier          #+#    #+#             */
/*   Updated: 2019/09/13 00:23:00 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static void		get_data_ptr(t_format *data, char *c, intmax_t n)
{
	*c = '\0';
	if (n < 0)
		*c = '-';
	else if (data->flag.opts & OPT_PLUS)
		*c = '+';
	else if (data->flag.opts & OPT_SPACE)
		*c = ' ';
}

static void		apply_specifier(t_format *data, intmax_t *n)
{
	if (*data->tail == 'D')
		*n = (long int)(*n);
	else if (data->flag.size_modifier & OPT_H)
		*n = (short)(*n);
	else if (data->flag.size_modifier & OPT_HH)
		*n = (signed char)(*n);
	else if (data->flag.size_modifier & OPT_LL)
		*n = (long long)(*n);
	else if (data->flag.size_modifier & OPT_L)
		*n = (long)(*n);
	else if (data->flag.size_modifier & OPT_J)
		*n = (intmax_t)(*n);
	else if (data->flag.size_modifier & OPT_Z)
		*n = (size_t)(*n);
	else
		*n = (int)(*n);
}

static void		apply_half_flag(t_format *data, char c, size_t len_cnb)
{
	size_t		len;

	len = (data->flag.precision > len_cnb) ? data->flag.precision : len_cnb;
	len = c ? len + 1 : len;
	if (c && !msk(data, OPT_MINUS)
			&& msk(data, OPT_ZERO) && !msk(data, OPT_PREC))
		write_char(data, c);
	if (!msk(data, OPT_MINUS))
	{
		while (data->flag.width > len && data->flag.width--)
			write_char(data, (msk(data, OPT_ZERO)
						&& !msk(data, OPT_PREC)) ? '0' : ' ');
	}
	if (c && (msk(data, OPT_MINUS)
				|| !msk(data, OPT_ZERO) || msk(data, OPT_PREC)))
		write_char(data, c);
	len = c ? len - 1 : len;
	while (len > len_cnb && len--)
		write_char(data, '0');
}

static void		write_d(t_format *data, size_t len, char *c_nb, intmax_t nb)
{
	if (!msk(data, OPT_PREC)
		|| (msk(data, OPT_PREC) && data->flag.precision > 0)
		|| (data->flag.precision == 0 && nb != 0) || nb > 0)
	{
		if (nb >= 0)
			get_aps(data, c_nb, len);
		else if (nb < 0)
		{
			write_char(data, c_nb[0]);
			get_aps(data, c_nb + 1, len - 1);
		}
	}
	else if (data->flag.width)
		write_char(data, ' ');
}

void			convert_d(t_format *data)
{
	intmax_t		nb;
	size_t			tab[2];
	char			*c_nb;
	char			c;

	nb = 0;
	conv_star(data);
	get_arg_n(data, &nb);
	apply_specifier(data, &nb);
	get_data_ptr(data, &c, nb);
	c_nb = ft_intmax_toa(nb);
	tab[0] = ft_strlen(c_nb);
	apply_half_flag(data, c, tab[0]);
	write_d(data, tab[0], c_nb, nb);
	if (data->flag.opts & OPT_MINUS)
	{
		tab[1] = data->flag.precision > tab[0] ? data->flag.precision : tab[0];
		tab[1] = c ? tab[1] + 1 : tab[1];
		while (data->flag.width > tab[1] && data->flag.width--)
			write_char(data, ' ');
	}
	free(c_nb);
}
