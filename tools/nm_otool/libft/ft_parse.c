/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_parse.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/05 22:09:19 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:47:01 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static char		*ft_strnchr(const char *s, int c, size_t len)
{
	while (*s && len--)
	{
		if (*s == c)
			return ((char *)s);
		s++;
	}
	return (NULL);
}

void			get_varg(t_format *data)
{
	create_node(&data->lst, data, *data->tail);
}

void			apply_flag(t_format *data)
{
	if (data->flag.opts & OPT_MINUS && *data->tail)
		write_char(data, *data->tail);
	while (data->flag.width-- > 1)
		write_char(data, (data->flag.opts & OPT_ZERO) ? '0' : ' ');
	if (!(data->flag.opts & OPT_MINUS) && *data->tail)
		write_char(data, *data->tail);
	if (!*data->tail)
		data->tail--;
}

void			ft_parse(t_format *data)
{
	int		i;
	char	*buff;

	i = 0;
	buff = NULL;
	while (*data->tail)
	{
		if (*data->tail == '%')
		{
			if (ft_strnchr(data->tail, '$', 5))
				get_valist(data);
			get_flags(data);
			if (!get_converter(data) && *data->tail)
				apply_flag(data);
			ft_memset(&data->flag, 0, sizeof(data->flag));
		}
		else if (*data->tail == '{' && get_color(data))
			;
		else
			write_char(data, *data->tail);
		if (*data->tail)
			data->tail++;
	}
	data->print_buff(data);
}

void			ft_preparse(t_format *data)
{
	char	*save;

	while (*data->tail)
	{
		if (*data->tail == '%')
		{
			save = data->tail;
			if (ft_strnchr(data->tail, '$', 5))
				get_valist(data);
			get_flags(data);
			if (ft_strnchr(save, '$', 5))
				get_varg(data);
			if (!(*data->tail == '%') && get_char_conv(data))
				get_varg(data);
			ft_memset(&data->flag, 0, sizeof(data->flag));
			data->flag.opts = 0x0;
			data->flag.size_modifier = 0;
		}
		data->tail++;
	}
}
