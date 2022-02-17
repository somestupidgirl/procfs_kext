/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strrchr.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/09 11:37:10 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/09 17:17:17 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char	*ft_strrchr(const char *s, int c)
{
	const char	*last;
	int			check;

	last = NULL;
	check = 0;
	while (*s)
	{
		if (*s == c)
		{
			check = 1;
			last = s;
		}
		s++;
	}
	if (check == 1)
		return ((char *)last);
	if (*s == c)
		return ((char *)s);
	return (NULL);
}
