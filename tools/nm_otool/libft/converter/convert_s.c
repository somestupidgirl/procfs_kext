/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_s.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 11:51:31 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/18 10:18:00 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

void			convert_s(t_format *data)
{
	char			*str;
	size_t			len;

	str = NULL;
	conv_star(data);
	get_arg_n(data, &str);
	if (str == NULL)
		str = "(null)";
	len = ft_strlen(str);
	if (msk(data, OPT_PREC) && data->flag.precision < len)
		len = data->flag.precision;
	if (!msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, msk(data, OPT_ZERO) ? '0' : ' ');
	write_nstr(data, str, len);
	if (msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, ' ');
}
