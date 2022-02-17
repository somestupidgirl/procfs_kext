/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_specifier.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/11 04:13:28 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/11 04:29:33 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

void				set_spec(t_format *data, int mask, int mask2)
{
	if (mask2)
	{
		if (data->flag.size_modifier & mask)
		{
			data->flag.size_modifier = 0;
			data->flag.size_modifier &= ~mask;
			data->flag.size_modifier |= mask2;
		}
		else
		{
			data->flag.size_modifier = 0;
			data->flag.size_modifier |= mask;
		}
	}
	else
	{
		data->flag.size_modifier = 0;
		data->flag.size_modifier |= mask;
	}
}

int					get_specifier(t_format *data)
{
	if (*data->tail == 'h')
		set_spec(data, OPT_H, OPT_HH);
	else if (*data->tail == 'l')
		set_spec(data, OPT_L, OPT_LL);
	else if (*data->tail == 'j')
		set_spec(data, OPT_J, 0);
	else if (*data->tail == 'z')
		set_spec(data, OPT_Z, 0);
	else
		return (1);
	return (0);
}
