/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_gr.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/31 20:47:02 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/18 10:19:35 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"
#include "converter.h"

char			*nprint_to_str_gr(char c)
{
	char		*str;

	if (!(str = (char *)malloc(sizeof(char) * 2)))
		return (NULL);
	if (c >= 0 && c < 16)
	{
		str[0] = 48;
		if (c > 9)
			str[1] = 65 + (c % 16) - 10;
		else
			str[1] = c % 16 + 48;
	}
	return (str);
}

void			convert_gr(t_format *data)
{
	char		*str;
	int			i;

	i = 0;
	conv_star(data);
	get_arg_n(data, &str);
	while (str[i])
	{
		if (ft_isprint(str[i]) == 0)
		{
			write_char(data, 92);
			write_nstr(data, nprint_to_str_gr(str[i]), 2);
		}
		else
			write_char(data, str[i]);
		i++;
	}
}
