/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_gc.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 23:35:20 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/18 10:19:28 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

void		convert_gc(t_format *data)
{
	wchar_t		c;
	size_t		len;

	conv_star(data);
	get_arg_n(data, &c);
	len = 1;
	if (!msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, msk(data, OPT_ZERO) ? '0' : ' ');
	ft_putwchar(data, c);
	if (msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, ' ');
}
