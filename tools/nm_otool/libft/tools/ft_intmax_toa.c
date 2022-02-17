/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_intmax_toa.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/09 20:57:48 by dbaffier          #+#    #+#             */
/*   Updated: 2019/09/13 00:18:24 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static uintmax_t	count_dec(int *nb_len, intmax_t n)
{
	uintmax_t	tmp;

	if (n < 0)
		n *= -1;
	tmp = (uintmax_t)n;
	while (tmp)
	{
		tmp /= 10;
		(*nb_len)++;
	}
	return ((uintmax_t)n);
}

char				*ft_intmax_toa(intmax_t n)
{
	int			nb_len;
	char		*res;
	uintmax_t	nb;

	nb_len = 0;
	if (n == 0)
		return (ft_strdup("0"));
	nb = count_dec(&nb_len, n);
	if (!(res = (char *)malloc(sizeof(char) * nb_len + 1)))
		return (NULL);
	res[nb_len] = '\0';
	while (nb_len--)
	{
		res[nb_len] = nb % 10 + 48;
		nb /= 10;
	}
	return (res);
}
