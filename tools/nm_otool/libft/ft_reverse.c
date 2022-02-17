/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_reverse.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/23 12:39:53 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/23 12:42:21 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char	*ft_reverse(char *str)
{
	int		len;
	int		i;

	i = 0;
	len = ft_strlen(str);
	while (i < len / 2)
	{
		ft_swap(&str[i], &str[len - i - 1]);
		i++;
	}
	return (str);
}
