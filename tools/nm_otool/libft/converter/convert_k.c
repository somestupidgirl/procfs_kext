/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_k.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/31 22:54:37 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/18 10:18:37 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_time.h"
#include "ft_printf.h"

void				write_nbr_buff(char *buff, unsigned int n, size_t *i)
{
	size_t			len;
	size_t			tmp_i;

	len = ft_intlen_base(n, 10) - 1;
	tmp_i = *i;
	while (n > 0)
	{
		buff[tmp_i + len] = n % 10 + 48;
		len--;
		(*i)++;
		n /= 10;
	}
}

void				write_char_buff(char *buff, char c, size_t *i)
{
	buff[*i] = c;
	*i = *i + 1;
}

void				write_str_buff(char *buff, char *str, size_t *i)
{
	int				j;

	j = 0;
	while (str[j])
	{
		buff[*i] = str[j];
		(*i)++;
		j++;
	}
}

size_t				fill_buff(char *buff, t_ftime *ptr)
{
	size_t			i;

	i = 0;
	write_nbr_buff(buff, ptr->year, &i);
	write_char_buff(buff, 32, &i);
	write_str_buff(buff, get_month(ptr->month), &i);
	write_char_buff(buff, 32, &i);
	write_str_buff(buff, get_day(ptr->n), &i);
	write_char_buff(buff, 32, &i);
	write_nbr_buff(buff, ptr->day, &i);
	write_str_buff(buff, " - ", &i);
	if (ptr->hour <= 9)
		write_char_buff(buff, 48, &i);
	write_nbr_buff(buff, ptr->hour, &i);
	write_char_buff(buff, 58, &i);
	if (ptr->min <= 9)
		write_char_buff(buff, 48, &i);
	write_nbr_buff(buff, ptr->min, &i);
	write_char_buff(buff, 58, &i);
	if (ptr->sec <= 9)
		write_char_buff(buff, 48, &i);
	write_nbr_buff(buff, ptr->sec, &i);
	return (i);
}

void				convert_k(t_format *data)
{
	unsigned int	n;
	static char		buff[30];
	t_ftime			ptr;
	size_t			len;

	ft_memset(&ptr, 0, sizeof(t_ftime));
	conv_star(data);
	get_arg_n(data, &n);
	get_date(n, &ptr);
	len = fill_buff(buff, &ptr);
	if (msk(data, OPT_PREC) && data->flag.precision < len)
		len = data->flag.precision;
	if (!msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, ' ');
	write_nstr(data, buff, len);
	if (msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, ' ');
}
