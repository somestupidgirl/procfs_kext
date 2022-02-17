/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_char_conv.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/06 22:42:55 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/11 05:52:05 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

char	get_char_conv(t_format *data)
{
	static t_convert	converter[] = {{'c', convert_c}, {'s', convert_s},
	{'d', convert_d}, {'p', convert_p}, {'u', convert_u}, {'x', convert_x},
		{'X', convert_gx}, {'U', convert_u}};
	size_t				i;

	i = 0;
	while (i < sizeof(converter) / sizeof(converter[0]))
	{
		if (converter[i].type == *data->tail)
			return (converter[i].type);
		i++;
	}
	return ('\0');
}
