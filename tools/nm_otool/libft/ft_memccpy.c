/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_memccpy.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/12 15:20:57 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/12 15:43:51 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	*ft_memccpy(void *dst, const void *src, int c, size_t n)
{
	unsigned char	*src2;
	unsigned char	*dst2;
	unsigned char	c2;

	src2 = (unsigned char *)src;
	dst2 = (unsigned char *)dst;
	c2 = (unsigned char)c;
	while (n--)
	{
		*dst2++ = *src2++;
		if (*(src2 - 1) == c2)
			return ((void *)dst2);
	}
	return (NULL);
}
