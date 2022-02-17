/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_ubase.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/08 02:50:48 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/11 03:19:52 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static void		stock_n(char **str, uintmax_t n, int base, char letter)
{
	int			mod;
	int			i;

	mod = 0;
	i = 0;
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
}

char			*ft_convert_ubase(uintmax_t n, int base,
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
