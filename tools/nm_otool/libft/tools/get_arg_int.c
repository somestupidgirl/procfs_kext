/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_arg_int.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/06 03:16:18 by dbaffier          #+#    #+#             */
/*   Updated: 2019/06/19 03:06:08 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"

void		getarg_uintmaxt(void *dst, va_list *args)
{
	uintmax_t	arg;

	arg = va_arg(*args, uintmax_t);
	if (dst)
		*((uintmax_t *)dst) = arg;
}

void		getarg_uint(void *dst, va_list *args)
{
	unsigned int	arg;

	arg = va_arg(*args, unsigned int);
	if (dst)
		*((unsigned int *)dst) = arg;
}

void		getarg_int(void *ptr, va_list *args)
{
	intmax_t			arg;

	arg = va_arg(*args, intmax_t);
	if (ptr)
		*((intmax_t *)ptr) = arg;
}

void		getarg_float(void *ptr, va_list *args)
{
	double	arg;

	arg = va_arg(*args, double);
	if (ptr)
		*((double *)ptr) = arg;
}
