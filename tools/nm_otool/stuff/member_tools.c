/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   member_tools.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/11/07 14:29:55 by dbaffier          #+#    #+#             */
/*   Updated: 2019/11/14 13:50:03 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

unsigned long	size_ar_name(struct ar_hdr *ar_hdr)
{
	long		i;

	i = sizeof(ar_hdr->ar_name) - 1;
	if (ar_hdr->ar_name[i] == ' ')
	{
		while (i > 0 && ar_hdr->ar_name[i] == ' ')
			i--;
	}
	return (i + 1);
}

unsigned long	ft_strtoul(const char *ptr, char **endptr, int base)
{
	unsigned long		n;

	n = 0;
	while (*ptr == ' ')
		ptr++;
	while (ptr && *ptr && ft_isdigit(*ptr))
	{
		n *= base;
		n += *ptr - 48;
		ptr++;
	}
	if (endptr == NULL)
		;
	return (n);
}
