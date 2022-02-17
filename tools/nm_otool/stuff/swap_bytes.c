/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   swap_bytes.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/27 23:55:58 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/07 20:22:40 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

uint16_t		swap_uint16(uint16_t val)
{
	return (val << 8) | (val >> 8);
}

int16_t			swap_int16(int16_t val)
{
	return ((val << 8) | ((val >> 8) & 0xFF));
}

uint32_t		swap_uint32(uint32_t val)
{
	return (((val & 0xFF00FF00) >> 8) | ((val & 0xFF00FF) << 8));
}

int32_t			swap_int32(int32_t val)
{
	return (((val) << 24) | (((val) << 8) & 0x00ff0000) |
			(((val) >> 8) & 0x0000ff00) |
			((unsigned int)(val) >> 24));
	return (((val & 0xFF00FF00) >> 8) | ((val & 0xFF00FF) << 8));
}

unsigned long	swap_long(unsigned int val)
{
	return (((val) << 24) | (((val << 8) & 0x00ff0000) |
				(((val) >> 8) & 0x0000ff00) |
				((unsigned long)(val) >> 24)));
}
