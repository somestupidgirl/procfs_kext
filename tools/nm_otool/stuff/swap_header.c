/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   swap_header.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/27 23:45:40 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/07 18:54:53 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"
#include <mach-o/fat.h>

static uint32_t		ft_swap_bytes(uint32_t num)
{
	return (((num & 0xff000000) >> 24) | ((num & 0x00ff0000) >> 8) |
			((num & 0x0000ff00) << 8) | ((num & 0x000000ff) << 24));
}

void				swap_fat_header(struct fat_header *hdr)
{
	hdr->magic = ft_swap_bytes(hdr->magic);
	hdr->nfat_arch = ft_swap_bytes(hdr->nfat_arch);
}

void				swap_fat_arch(struct fat_arch *arch,
		unsigned long n_fat_arch)
{
	unsigned long	i;

	i = 0;
	while (i < n_fat_arch)
	{
		arch[i].cputype = ft_swap_bytes(arch[i].cputype);
		arch[i].cpusubtype = ft_swap_bytes(arch[i].cpusubtype);
		arch[i].offset = ft_swap_bytes(arch[i].offset);
		arch[i].size = ft_swap_bytes(arch[i].size);
		arch[i].align = ft_swap_bytes(arch[i].align);
		i++;
	}
}

void				swap_nlist(struct nlist *symbols, uint32_t nsymbols)
{
	uint32_t		i;

	i = 0;
	while (i < nsymbols)
	{
		symbols[i].n_un.n_strx = swap_int32(symbols[i].n_un.n_strx);
		symbols[i].n_desc = swap_short(symbols[i].n_desc);
		symbols[i].n_value = swap_int32(symbols[i].n_value);
		i++;
	}
}

void				swap_nlist_64(struct nlist_64 *symbols, uint32_t nsymbols)
{
	uint32_t		i;

	i = 0;
	while (i < nsymbols)
	{
		symbols[i].n_un.n_strx = swap_int32(symbols[i].n_un.n_strx);
		symbols[i].n_desc = swap_short(symbols[i].n_desc);
		symbols[i].n_value = swap_long_long(symbols[i].n_value);
		i++;
	}
}
