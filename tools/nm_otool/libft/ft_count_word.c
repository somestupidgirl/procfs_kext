/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_count_word.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/23 15:10:15 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/23 15:20:30 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

int		ft_count_word(char *str)
{
	int		word;
	int		check;

	check = 0;
	word = 0;
	if (!(str = ft_strtrim(str)))
		return (0);
	while (str && *str)
	{
		while (*str == 32)
		{
			check = 1;
			str++;
		}
		if (check == 1)
		{
			check = 0;
			word++;
		}
		str++;
	}
	word++;
	return (word);
}
