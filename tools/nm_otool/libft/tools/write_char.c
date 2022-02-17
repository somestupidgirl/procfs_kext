/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   write_char.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 02:38:13 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/11 03:59:56 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

void	write_str(t_format *data, char *str)
{
	size_t		i;

	i = 0;
	if (!str)
		return ;
	while (str[i])
		write_char(data, str[i++]);
}

void	write_nstr(t_format *data, char *str, size_t len)
{
	if (!str)
		return ;
	while (len)
	{
		write_char(data, *str++);
		len--;
	}
}

void	write_char(t_format *data, char c)
{
	data->buff[data->pos++] = c;
	if (data->pos >= BUFF_SIZE)
	{
		data->print_buff(data);
		data->pos = 0;
	}
}

void	write_float(t_format *data, char c, int j)
{
	if (!(data->pos >= BUFF_SIZE || data->tmp_pos + data->pos >= BUFF_SIZE))
	{
		data->buff[data->tmp_pos + j] = c;
		data->pos++;
	}
	else
	{
		data->print_buff(data);
		data->pos = 0;
	}
}

void	write_nbr(t_format *data, uintmax_t base, uintmax_t n, char letter)
{
	if (n <= base - 1)
	{
		if (n <= 9)
			write_char(data, n + 48);
		else
			write_char(data, (n - 10) + letter);
	}
	if (n > base - 1)
	{
		write_nbr(data, base, n / base, letter);
		write_nbr(data, base, n % base, letter);
	}
}
