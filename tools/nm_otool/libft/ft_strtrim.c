/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strtrim2.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/23 14:00:20 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/11 04:43:34 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static int			ft_space(int c)
{
	return ((c == 32 || c == 10 || c == 9) ? 1 : 0);
}

char				*ft_strtrim(char const *s)
{
	char	*d;
	size_t	i;
	size_t	len;
	size_t	end;

	if (!s)
		return (NULL);
	len = ft_strlen(s);
	end = 0;
	i = 0;
	while (ft_space(s[--len]))
		end++;
	if (end == ft_strlen(s))
		return (ft_strnew(0));
	while (ft_space(s[i]))
		i++;
	if (!(d = ft_strsub(s, i, (ft_strlen(s) - (end + i)))))
		return (NULL);
	return (d);
}
