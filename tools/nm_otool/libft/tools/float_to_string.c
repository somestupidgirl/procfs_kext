/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   float_to_string.c                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/30 18:12:44 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/12 00:15:09 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"
#include "converter.h"

void				double_to_int(t_convf *ptr)
{
	long double			tmp_n;
	unsigned long long	ull_max;
	int					tmp_len;
	int					i;

	i = 0;
	tmp_len = ptr->d_len;
	tmp_n = (long double)ptr->n;
	ull_max = -1;
	while (i < tmp_len && (tmp_n * 10) < ull_max)
	{
		tmp_n *= 10;
		i++;
	}
	ptr->ni = (unsigned long long)(tmp_n + 0.5);
	ptr->ni_size = ft_intlen_base(ptr->ni, 10);
	ptr->n_zero = (i - ptr->ni_size > 0) ? i - ptr->ni_size : 0;
	ptr->d_pos = tmp_len - i;
	ptr->cast = (unsigned long)(tmp_n + 0.5);
	while (i)
	{
		ptr->cast /= 10;
		i--;
	}
}

void				is_zero(t_format *data, size_t len)
{
	write_char(data, 48);
	if (len != 0 || (data->flag.opts & OPT_HASH && len == 0))
		write_char(data, 46);
	while (len)
	{
		write_char(data, 48);
		len--;
	}
}

void				int_part_conv(t_format *data, t_convf *ptr, size_t len)
{
	unsigned int	i;
	unsigned int	dot;
	unsigned int	tlen;

	i = 0;
	dot = 1;
	len = (data->flag.opts & OPT_APS) ? (len + (len - 1) / 3) : len;
	tlen = len - 1;
	while (i < len)
	{
		write_float(data, (ptr->cast % (unsigned long)ptr->base) + 48, tlen--);
		i++;
		dot++;
		ptr->cast = ptr->cast / (unsigned long)ptr->base;
		if (msk(data, OPT_APS) && dot == 4 && i < len - 1)
		{
			write_float(data, 46, tlen--);
			i++;
			dot = 1;
		}
	}
	if (!(msk(data, OPT_PREC) && data->flag.precision == 0)
			|| (msk(data, OPT_HASH) && data->flag.precision == 0))
		write_char(data, 46);
}

void				dot_part_conv(t_format *data, t_convf *ptr)
{
	size_t		i;
	size_t		len;
	size_t		prec;

	i = 0;
	data->tmp_pos = data->pos;
	if (data->flag.opts & OPT_PREC && (ptr->ni_size + ptr->n_zero) > ptr->d_len)
		prec = ptr->d_len;
	else
		prec = ptr->ni_size + ptr->n_zero;
	len = ptr->ni_size + ptr->n_zero - 1;
	while (i < prec)
	{
		write_float(data, ptr->ni % (unsigned long long)ptr->base + 48, len);
		ptr->ni /= ptr->base;
		len--;
		i++;
	}
	if (i < ptr->d_len)
		while (i < ptr->d_len)
		{
			write_char(data, '0');
			i++;
		}
}

char				*float_to_string(t_format *data, t_convf *ptr)
{
	data->tmp_pos = data->pos;
	if (ptr->n == 0.0)
		is_zero(data, ptr->d_len);
	else
	{
		double_to_int(ptr);
		int_part_conv(data, ptr, ft_intlen_base(ptr->cast, 10));
		dot_part_conv(data, ptr);
	}
	return (NULL);
}
