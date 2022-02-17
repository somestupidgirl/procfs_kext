/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_itoa.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/23 12:36:21 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/09 21:10:16 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static int		count_dec(int *n, int *sign)
{
	int		nb_len;
	int		tmp;

	nb_len = 2;
	tmp = *n;
	if (*n < 0)
	{
		*n *= -1;
		*sign = 1;
	}
	while (tmp)
	{
		tmp /= 10;
		nb_len++;
	}
	return (nb_len);
}

char			*ft_itoa(int n)
{
	char	*res;
	int		nb_len;
	int		sign;

	if (n == -2147483648)
		return (ft_strdup("-2147483648"));
	if (n == 0)
		return (ft_strdup("0"));
	sign = 0;
	nb_len = count_dec(&n, &sign);
	if (!(res = (char *)malloc(sizeof(char) * nb_len)))
		return (NULL);
	if (sign == 0)
		nb_len--;
	res[--nb_len] = '\0';
	while (nb_len--)
	{
		res[nb_len] = n % 10 + 48;
		n /= 10;
	}
	if (sign)
		res[0] = '-';
	return (res);
}
