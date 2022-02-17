/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_sort_all.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/26 14:52:05 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/26 14:56:57 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	ft_sort_all(void *tab, int (*cmp)(void*, void*), int size, size_t type)
{
	void	*save;
	void	*tmp;
	int		i;
	int		check;

	check = 1;
	save = tab;
	while (size > 1 && check)
	{
		check = 0;
		i = 0;
		while (i < size - 1)
		{
			tmp = tab + type;
			if ((*cmp)(tab, tmp) > 0)
			{
				ft_swap_all(tab, tmp, type);
				check = 1;
			}
			tab += type;
			i++;
		}
		tab = save;
	}
}
