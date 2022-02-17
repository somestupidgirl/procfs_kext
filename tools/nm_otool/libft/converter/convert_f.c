/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_f.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 11:48:47 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/18 10:18:28 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"
#include "converter.h"

static int		is_unreal(t_format *data, double n)
{
	if (n == 1.0 / 0.0)
	{
		if (msk(data, OPT_PLUS))
			data->flag.width += 4;
		write_str(data, "inf");
	}
	else if (n == 1.0 / -0.0)
	{
		data->flag.width += 4;
		write_str(data, "inf");
	}
	else if (n != n)
	{
		data->flag.width += 3;
		write_str(data, "nan");
	}
	else
		return (0);
	return (1);
}

static void		apply_sign_flags(t_format *data, double n)
{
	if ((n < 0 || 1.0 / n < 0) && !msk(data, OPT_ZERO))
		write_char(data, 45);
	if (msk(data, OPT_PLUS) && (n > 0 || 1.0 / n > 0))
		write_char(data, 43);
}

static void		apply_half_flag(t_format *data,
					t_convf *ptr, int cond, double n)
{
	if (msk(data, OPT_PLUS) && ((n > 0) || 1.0 / n > 0) && data->flag.width)
		data->flag.width--;
	if (msk(data, OPT_SPACE) && !(msk(data, OPT_PREC)
				|| msk(data, OPT_PLUS)) && data->flag.width < ptr->n_len)
		write_char(data, 32);
	if (msk(data, OPT_SPACE) && data->flag.width > ptr->n_len &&
			(n > 0 || 1.0 / n > 0))
	{
		write_char(data, 32);
		data->flag.width--;
	}
	if (msk(data, OPT_ZERO) && (n < 0 || 1.0 / n < 0))
		write_char(data, 45);
	if (!msk(data, OPT_MINUS) && data->flag.precision < data->flag.width)
	{
		if (ptr->n_len < data->flag.width)
		{
			data->flag.width -= ptr->n_len;
			while (data->flag.width--)
				write_char(data, cond ? 48 : 32);
		}
	}
	apply_sign_flags(data, n);
}

static void		apply_specifier(t_format *data, double *n)
{
	if (data->flag.size_modifier & OPT_LL)
		*n = (long double)(*n);
	else if (data->flag.size_modifier & OPT_L)
		*n = (long double)(*n);
}

void			convert_f(t_format *data)
{
	t_convf		ptr;
	double		n;
	int			cond;

	n = 0.0;
	cond = msk(data, OPT_ZERO);
	ft_memset(&ptr, 0, sizeof(ptr));
	conv_star(data);
	get_arg_n(data, &n);
	apply_specifier(data, &n);
	ptr.n = (long double)n;
	ptr.base = 10;
	ptr.d_len = msk(data, OPT_PREC) ? data->flag.precision : 6;
	ft_doublelen(&ptr);
	apply_half_flag(data, &ptr, cond, n);
	if (is_unreal(data, n) == 0)
		float_to_string(data, &ptr);
	if (msk(data, OPT_MINUS) && data->flag.precision < data->flag.width)
	{
		data->flag.width -= ptr.n_len;
		while (data->flag.width--)
			write_char(data, 32);
	}
}
