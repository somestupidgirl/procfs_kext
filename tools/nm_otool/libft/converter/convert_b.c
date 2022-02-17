/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_b.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/31 17:46:57 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/18 10:18:07 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"
#include "converter.h"

void				convert_b(t_format *data)
{
	unsigned int	n;

	n = 0;
	conv_star(data);
	if (msk(data, OPT_HASH))
		write_str(data, "0b");
	get_arg_n(data, &n);
	write_nbr(data, 2, n, 97);
}
