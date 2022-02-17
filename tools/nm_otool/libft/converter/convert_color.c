/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   convert_color.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 12:02:30 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/11 05:53:34 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static int	is_color(char *cmp, size_t n)
{
	static t_color	tab[] = {{"WHITE", "\033[0m"}, {"BLACK", "\033[30m"},
	{"RED", "\033[31m"}, {"GREEN", "\033[32m"}, {"YELLOW", "\033[33m"},
		{"BLUE", "\033[34m"}, {"PINK", "\033[35m"}, {"CYAN", "\033[36m"},
		{"GREY", "\033[37m"}};
	size_t			i;

	i = 0;
	while (i < sizeof(tab) / sizeof(*tab))
	{
		if (!ft_strncmp(cmp, tab[i].color, n))
			return (1);
		i++;
	}
	return (0);
}

size_t		color_end(const char *str)
{
	size_t		pos;

	pos = 1;
	while (str[pos])
	{
		if (pos > 7)
			return (0);
		if (str[pos] == '%')
			return (0);
		if (str[pos] == '}')
			break ;
		pos++;
	}
	if (is_color((char *)&str[1], pos - 1))
		return (1);
	return (0);
}

static char	*get_chunk(t_format *data, char end)
{
	int		i;
	char	*chunk;

	i = 0;
	if (!(chunk = malloc(sizeof(char) * 8)))
		return (NULL);
	data->tail++;
	while (*data->tail != end)
	{
		chunk[i] = *data->tail;
		i++;
		data->tail++;
	}
	chunk[i] = 0;
	return (chunk);
}

char		*color_specifier(const char *cmp)
{
	static t_color	tab[] = {{"WHITE", "\033[0m"}, {"BLACK", "\033[30m"},
	{"RED", "\033[31m"}, {"GREEN", "\033[32m"}, {"YELLOW", "\033[33m"},
		{"BLUE", "\033[34m"}, {"PINK", "\033[35m"}, {"CYAN", "\033[36m"},
		{"GREY", "\033[37m"}};
	size_t			i;

	i = 0;
	while (i < sizeof(tab) / sizeof(*tab))
	{
		if (!ft_strcmp(cmp, tab[i].color))
			return (tab[i].code);
		i++;
	}
	return (NULL);
}

int			get_color(t_format *data)
{
	char		*color;
	char		*chunk;
	size_t		color_size;

	color_size = 0;
	if (color_end(data->tail))
	{
		if ((chunk = get_chunk(data, '}')))
		{
			if ((color = color_specifier(chunk)))
				write_str(data, color);
			free(chunk);
			return (1);
		}
	}
	return (0);
}
