/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   apostrophe_flag.c                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/05 21:32:53 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/08 03:06:59 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

void			get_aps(t_format *data, char *str, unsigned int len)
{
	unsigned int n_dots;
	unsigned int i;

	i = 0;
	if (len > 3)
	{
		n_dots = (len - 1) / 3;
		while (len)
		{
			if (data->flag.opts & OPT_APS && (len - 1) % 3 == 0 && n_dots != 0)
			{
				n_dots--;
				write_char(data, str[i]);
				write_char(data, 46);
			}
			else
				write_char(data, str[i]);
			i++;
			len--;
		}
	}
	else
		write_nstr(data, str, len);
}
