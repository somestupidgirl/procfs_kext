/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_memcpy.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/12 15:02:06 by mmonier           #+#    #+#             */
/*   Updated: 2019/07/16 02:52:47 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	*ft_memcpy(void *dst, const void *src, size_t n)
{
	char	*src2;
	char	*dst2;

	src2 = (char *)src;
	dst2 = (char *)dst;
	if (n == 0 || dst == src)
		return ((void *)dst);
	while (n != 0)
	{
		*dst2++ = *src2++;
		n--;
	}
	return ((void *)dst);
}
