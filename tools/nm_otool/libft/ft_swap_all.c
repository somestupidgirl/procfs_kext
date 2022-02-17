/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_swap_all.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/26 14:52:23 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/26 15:02:09 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	ft_swap_all(void *a, void *b, size_t type)
{
	void *tmp;

	if ((tmp = malloc(type)))
	{
		ft_memcpy(tmp, a, type);
		ft_memcpy(a, b, type);
		ft_memcpy(b, tmp, type);
		free(tmp);
	}
}
