/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   converter.h                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/30 00:47:11 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/11 03:53:18 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONVERTER_H
# define CONVERTER_H

# include <stdlib.h>

typedef struct				s_convf
{
	long double				n;
	unsigned long long		ni;
	unsigned long			cast;
	size_t					ni_size;
	size_t					n_zero;
	int						sign;
	size_t					d_len;
	size_t					n_len;
	int						base;
	size_t					d_pos;
}							t_convf;

#endif
