/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   len_func.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/30 20:32:34 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/11 03:29:25 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"
#include "converter.h"

void			ft_doublelen(t_convf *ptr)
{
	size_t		len;
	int			tmp_len;
	int			ni;

	tmp_len = ptr->d_len;
	len = 0;
	ni = (int)ptr->n;
	ptr->n_len = (ptr->n == 0) ? ptr->d_len + 1 : ptr->n_len;
	len = (1.0 / ptr->n < 0) ? len + 1 : len;
	if (ptr->n < 0)
	{
		ptr->n *= -1;
		ptr->sign = 1;
		len += 1;
	}
	len = (ni == 0) ? len + 1 : len;
	while (ni > 0)
	{
		ni = ni / 10;
		len++;
	}
	ptr->n_len = (ptr->d_len != 0) ? len + ptr->d_len + 1 : len + ptr->d_len;
}

size_t			ft_intlen_base(intmax_t n, int base)
{
	size_t		len;

	len = 0;
	if (n == 0)
		return (1);
	if (n < 0)
		n *= -1;
	while (n > 0)
	{
		n /= base;
		len++;
	}
	return (len);
}

size_t			ft_uintlen_base(uintmax_t n, int base)
{
	size_t		len;

	len = 0;
	if (n == 0)
		return (1);
	while (n > 0)
	{
		n /= base;
		len++;
	}
	return (len);
}
