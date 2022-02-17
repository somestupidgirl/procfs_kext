/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_converter.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/06 23:32:03 by dbaffier          #+#    #+#             */
/*   Updated: 2019/09/13 00:16:30 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

void			convert_m(t_format *data)
{
	size_t		len;

	len = 1;
	if (!(data->flag.opts & OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, data->flag.opts & OPT_ZERO ? '0' : ' ');
	write_char(data, '%');
	if (data->flag.opts & OPT_MINUS)
		while (data->flag.width > len && data->flag.width--)
			write_char(data, ' ');
}

int				get_converter(t_format *data)
{
	static t_convert	converter[] = {{'c', convert_c}, {'s', convert_s},
	{'d', convert_d}, {'p', convert_p}, {'u', convert_u}, {'x', convert_x},
		{'X', convert_gx}, {'U', convert_u}};
	size_t				i;

	i = 0;
	while (i < sizeof(converter) / sizeof(converter[0]))
	{
		if (converter[i].type == *data->tail)
		{
			converter[i].func(data);
			return (1);
		}
		i++;
	}
	return (0);
}
