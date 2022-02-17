/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_c.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 11:51:48 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/18 10:18:12 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

void			convert_c(t_format *data)
{
	intmax_t		c;
	size_t			len;

	c = 0;
	conv_star(data);
	get_arg_n(data, &c);
	len = 1;
	if (!msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, msk(data, OPT_ZERO) ? '0' : ' ');
	write_char(data, c);
	if (msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, ' ');
}
