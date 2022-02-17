/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_memmove.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/12 16:34:34 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/12 17:16:32 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	*ft_memmove(void *dst, const void *src, size_t len)
{
	char	*dst2;
	char	*src2;

	dst2 = (char *)dst;
	src2 = (char *)src;
	if (dst == src)
		return (dst);
	if (dst > src)
	{
		src2 += len - 1;
		dst2 += len - 1;
		while (len != 0)
		{
			*dst2-- = *src2--;
			len--;
		}
	}
	else
		while (len != 0)
		{
			*dst2++ = *src2++;
			len--;
		}
	return (dst);
}
