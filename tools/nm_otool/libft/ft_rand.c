/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_rand.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/18 05:06:42 by dbaffier          #+#    #+#             */
/*   Updated: 2019/09/13 22:02:45 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

int			ft_rand(int min, int max, void *random)
{
	int		n;
	int		rand;

	rand = (int)random;
	n = (int)random;
	n = (n < 0) ? -n : n;
	while (n >= 10)
	{
		rand += (n % 10);
		n /= 10;
	}
	rand = (rand % (max - min + 1)) + min;
	return (rand);
}
