/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   write_wchar.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 17:48:04 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/11 05:56:23 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

size_t	ft_wstrlen(const wchar_t *str)
{
	size_t	i;
	size_t	len;

	i = 0;
	len = 0;
	if (!str)
		return (0);
	while (str[i])
	{
		if (str[i] <= 0x7F)
			len += 1;
		else if (str[i] <= 0x7FF)
			len += 2;
		else if (str[i] <= 0xFFFF)
			len += 3;
		else
			len += 4;
		i++;
	}
	return (len);
}

void	write_wchar(t_format *data, wchar_t c)
{
	data->buff[data->pos++] = c;
	if (data->pos >= BUFF_SIZE)
	{
		data->print_buff(data);
		data->pos = 0;
	}
}

void	ft_putwchar(t_format *data, wchar_t c)
{
	if (c <= 127)
		write_char(data, c);
	else if (c <= 2047)
	{
		write_char(data, ((c >> 6) | 0xC0));
		write_char(data, (c & 0x3F) | 0x80);
	}
	else if (c <= 65535)
	{
		write_char(data, (c >> 12) | 0xE0);
		write_char(data, ((c >> 6) & 0x3F) | 0x80);
		write_char(data, (c & 0x3F) | 0x80);
	}
	else
	{
		write_char(data, (c >> 18) | 0xF0);
		write_char(data, ((c >> 12) & 0x3F) | 0x80);
		write_char(data, ((c >> 6) & 0x3F) | 0x80);
		write_char(data, (c & 0x3F) | 0x80);
	}
}

size_t	wchar_len(wchar_t w)
{
	if (w <= 0x7F)
		return (1);
	if (w <= 0x7FF)
		return (2);
	if (w <= 0xFFFF)
		return (3);
	return (4);
}

void	ft_putwstr(t_format *data, wchar_t *str, size_t len)
{
	size_t		i;
	size_t		weight;

	i = 0;
	weight = 0;
	if (!str)
		return ;
	while (str[i] && weight < len)
	{
		weight += wchar_len(str[i]);
		if (weight <= len)
			ft_putwchar(data, str[i]);
		i++;
	}
}
