/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   print_fat.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/27 23:21:23 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:55:13 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"
#include <mach-o/fat.h>

void	print_fat_headers(struct fat_header *hdr,
		struct fat_arch *arc, unsigned long size)
{
	long		i;

	i = -1;
	(void)size;
	ft_printf("Fat headers\n");
	ft_printf("fat_magic %#x\n", (unsigned long)(hdr->magic));
	ft_printf("nfat_arch %lu", hdr->nfat_arch);
	ft_printf("\n");
	while (++i < hdr->nfat_arch)
	{
		ft_printf("architecture ");
		ft_printf("%u\n", i);
		ft_printf("    cputype %d\n", arc[i].cputype);
		ft_printf("    cpusubtype %d\n", arc[i].cpusubtype & ~CPU_SUBTYPE_MASK);
		ft_printf("    capabilities 0x%x\n", (unsigned int)
			((arc[i].cpusubtype & CPU_SUBTYPE_MASK) >> 24));
		ft_printf("    offset %d", arc[i].offset);
		ft_printf("\n");
		ft_printf("    size %u\n", arc[i].size);
		ft_printf("    align 2^%lu (%d)\n", arc[i].align, 1 << arc[i].align);
	}
}
