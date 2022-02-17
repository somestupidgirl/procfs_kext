/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   byte_sex.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/27 22:04:03 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:44:47 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

enum e_byte_sex		get_host_byte_sex(void)
{
	uint32_t	s;

	s = (BIG_ENDIAN_BYTE_SEX << 24) | LITTLE_ENDIAN_BYTE_SEX;
	return ((enum e_byte_sex)*((char *)&s));
}

int					ft_ar_name(struct ar_hdr *hdr, char **name, size_t *len)
{
	if (ft_strncmp(hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1))
	{
		*name = hdr->ar_name;
		*len = 0;
		while (hdr->ar_name[*len] && *len < sizeof(hdr->ar_name))
			++*len;
		return (0);
	}
	*name = (char *)(hdr + 1);
	*len = ft_atoi(hdr->ar_name + sizeof(AR_EFMT1) - 1);
	return (1);
}
