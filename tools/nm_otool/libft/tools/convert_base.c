/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_base.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/30 20:40:10 by mmonier           #+#    #+#             */
/*   Updated: 2019/11/07 14:50:36 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static void		stock_n(char **str, intmax_t n, int base, char letter)
{
	int			mod;
	int			i;
	int			sign;

	mod = 0;
	i = 0;
	sign = 0;
	if (n < 0)
	{
		sign = 1;
		n *= -1;
	}
	while (n > 0)
	{
		mod = n % base;
		if (mod > 9)
			(*str)[i] = letter + (mod - 10);
		else
			(*str)[i] = mod + 48;
		i++;
		n = n / base;
	}
	(*str)[i] = '\0';
	ft_reverse(*str);
	*str = (sign == 1) ? ft_strjoinfree("-", *str, 2) : *str;
}

char			*ft_convert_base(intmax_t n, int base,
		size_t n_size, char letter)
{
	char	*str;

	if (!(str = (char *)malloc(sizeof(char) * n_size + 1)))
		return (NULL);
	if (n == 0)
	{
		str[0] = 48;
		str[1] = '\0';
	}
	else
		stock_n(&str, n, base, letter);
	return (str);
}
