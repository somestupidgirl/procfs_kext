/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_printf.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/29 12:33:31 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:48:45 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

static void		ft_print_buff(t_format *data)
{
	data->ret += data->pos;
	write(STDOUT_FILENO, data->buff, data->pos);
	data->pos = 0;
}

static void		free_lst(t_va_list *head)
{
	if (head)
	{
		free_lst(head->next);
		free(head);
	}
}

int				ft_printf(const char *str, ...)
{
	t_format	data;
	int			nb_char;

	nb_char = 0;
	ft_memset(&data, 0, sizeof(data));
	data.tail = (char *)str;
	data.print_buff = ft_print_buff;
	data.id = 1;
	va_start(data.arg, str);
	va_copy(data.copy, data.arg);
	ft_preparse(&data);
	data.tail = (char *)str;
	ft_parse(&data);
	va_end(data.arg);
	free_lst(data.lst);
	return (data.ret);
}
