/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strjoinfree.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/11 04:39:39 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/11 04:39:41 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char	*ft_strjoinfree(char *s1, char *s2, int i)
{
	char *str;

	str = ft_strjoin(s1, s2);
	if (i == 1)
		free(s1);
	else if (i == 2)
		free(s2);
	else if (i == 3)
	{
		free(s1);
		free(s2);
	}
	return (str);
}
