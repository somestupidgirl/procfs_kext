/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_gs.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 17:44:49 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/18 10:19:44 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static int		get_end_prec(size_t *ret, const wchar_t *w, const size_t max)
{
	if (*w <= 0x7FF)
	{
		if (*ret + 2 > max)
			return (1);
		*ret = *ret + 2;
	}
	else if (*w <= 0xFFFF)
	{
		if (*ret + 3 > max)
			return (1);
		*ret += 3;
	}
	else if (*w <= 0x10FFFF)
	{
		if (*ret + 4 > max)
			return (1);
		*ret += 4;
	}
	return (0);
}

static size_t	get_precision(const wchar_t *str, const size_t max)
{
	size_t		i;
	size_t		ret;

	i = 0;
	ret = 0;
	if (!str)
		return (0);
	while (str && str[i])
	{
		if (str[i] <= 0x7F)
		{
			if (ret + 1 > max)
				return (ret);
			ret++;
		}
		else if (get_end_prec(&ret, &str[i], max))
			return (ret);
		i++;
	}
	return (ret);
}

void			convert_gs(t_format *data)
{
	wchar_t			*str;
	size_t			len;

	conv_star(data);
	str = NULL;
	get_arg_n(data, &str);
	if (str == NULL)
		str = L"(null)";
	len = ft_wstrlen(str);
	if (msk(data, OPT_PREC) && data->flag.precision < len)
		len = get_precision(str, data->flag.precision);
	if (!msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, msk(data, OPT_ZERO) ? '0' : ' ');
	ft_putwstr(data, str, len);
	if (msk(data, OPT_MINUS))
		while (data->flag.width > len && data->flag.width--)
			write_char(data, ' ');
}
