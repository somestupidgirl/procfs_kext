/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strjoin_c.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/11 04:39:55 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/11 04:39:57 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char	*ft_strjoin_c(char const *s1, char c)
{
	char	*tmp;
	int		i;

	i = 0;
	if (!s1)
		return (NULL);
	if (!(tmp = (char *)malloc(sizeof(char) * (ft_strlen(s1) + 2))))
		return (NULL);
	while (s1[i])
	{
		tmp[i] = s1[i];
		i++;
	}
	tmp[i] = c;
	tmp[i + 1] = '\0';
	free((void *)s1);
	return (tmp);
}
