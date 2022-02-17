/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_utoa.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/09 01:01:45 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/09 21:23:53 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static int		count_dec(int *n)
{
	int		nb_len;
	int		tmp;

	nb_len = 2;
	tmp = *n;
	if (*n < 0)
		*n *= -1;
	while (tmp)
	{
		tmp /= 10;
		nb_len++;
	}
	return (nb_len);
}

char			*ft_utoa(int n)
{
	char	*res;
	int		nb_len;

	if (n == -2147483648)
		return (ft_strdup("2147483648"));
	if (n == 0)
		return (ft_strdup("0"));
	nb_len = count_dec(&n) - 1;
	if (!(res = (char *)malloc(sizeof(char) * nb_len)))
		return (NULL);
	res[--nb_len] = '\0';
	while (nb_len--)
	{
		res[nb_len] = n % 10 + 48;
		n /= 10;
	}
	return (res);
}
