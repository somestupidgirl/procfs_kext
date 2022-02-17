/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_touppercase.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/11 04:40:10 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/11 04:40:24 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char		*ft_touppercase(char *str)
{
	int		i;
	char	*n_str;

	if (!(n_str = (char *)malloc(sizeof(char) * ft_strlen(str))))
		return (NULL);
	i = 0;
	while (str[i])
	{
		if (str[i] >= 97 && str[i] <= 122)
			n_str[i] = str[i] - 32;
		else
			n_str[i] = str[i];
		i++;
	}
	return (n_str);
}
