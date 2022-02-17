/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_flags.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 14:12:54 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/22 14:21:03 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static int		ft_atoi_pf(t_format *data)
{
	int			res;

	res = 0;
	while (*data->tail && ft_isdigit(*data->tail))
	{
		res = res * 10;
		res = res + *data->tail - '0';
		data->tail++;
	}
	data->tail--;
	return (res);
}

void			get_precision(t_format *data)
{
	data->flag.opts |= OPT_PREC;
	data->tail++;
	if (*data->tail == '*')
	{
		data->flag.star = '*';
		create_node(&data->lst, data, *data->tail);
	}
	else
		data->flag.precision = ft_atoi_pf(data);
}

void			get_star(t_format *data)
{
	if (data->flag.width > 0)
		data->flag.width = 0;
	data->flag.opts |= OPT_HSTAR;
	create_node(&data->lst, data, *data->tail);
}

void			get_flags(t_format *data)
{
	while (*(++data->tail))
	{
		if (*data->tail == '#')
			data->flag.opts |= OPT_HASH;
		else if (*data->tail == '-')
			data->flag.opts |= OPT_MINUS;
		else if (*data->tail == '+')
			data->flag.opts |= OPT_PLUS;
		else if (*data->tail == ' ')
			data->flag.opts |= OPT_SPACE;
		else if (*data->tail == '0')
			data->flag.opts |= OPT_ZERO;
		else if (*data->tail == '\'')
			data->flag.opts |= OPT_APS;
		else if (ft_isdigit(*data->tail))
			data->flag.width = ft_atoi_pf(data);
		else if (*data->tail == '.')
			get_precision(data);
		else if (*data->tail == '*')
			get_star(data);
		else if (get_specifier(data))
			break ;
	}
}
