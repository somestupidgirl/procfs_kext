/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_r.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/31 19:50:11 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/18 10:19:03 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"
#include "converter.h"

void			nprint_to_str(char c, char *conv)
{
	if (c >= 0 && c < 16)
	{
		*(conv + 0) = 48;
		if (c > 9)
			*(conv + 1) = 97 + (c % 16) - 10;
		else
			*(conv + 1) = c % 16 + 48;
	}
}

void			convert_r(t_format *data)
{
	char		*str;
	static char	conv[2];
	int			i;

	i = 0;
	get_arg_n(data, &str);
	while (str[i])
	{
		if (ft_isprint(str[i]) == 0)
		{
			write_char(data, 92);
			nprint_to_str(str[i], conv);
			write_nstr(data, conv, 2);
			ft_memset(&conv, 0, sizeof(conv));
		}
		else
			write_char(data, str[i]);
		i++;
	}
}
