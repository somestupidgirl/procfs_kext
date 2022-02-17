/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_args.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/03 20:06:02 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/11 05:53:46 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

void		getarg_ptr(void *ptr, va_list *args)
{
	void	*arg;

	arg = va_arg(*args, void *);
	if (ptr)
		*((void **)ptr) = arg;
}

void		getarg_null(void *dst, va_list *args)
{
	dst = NULL;
	args = NULL;
}

void		getarg_wchar(void *dst, va_list *args)
{
	wchar_t		arg;

	arg = va_arg(*args, wchar_t);
	if (dst)
		*((wchar_t *)dst) = arg;
}

void		getarg_wstr(void *dst, va_list *args)
{
	wchar_t		*arg;

	arg = va_arg(*args, wchar_t *);
	if (dst)
		*((wchar_t **)dst) = arg;
}
