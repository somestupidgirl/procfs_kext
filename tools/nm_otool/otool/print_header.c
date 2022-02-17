/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   print_header.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/08 22:27:57 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:22:04 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_otool.h"

void		print_header(t_ofile *of)
{
	ft_printf("%s", of->file_name);
	if (of->member_ar_hdr)
		ft_printf("(%.*s):\n", (int)of->member_name_size, of->member_name);
	else
		ft_printf(":\n");
}

void		print_mach_header(struct mach_header *mh)
{
	ft_printf("Mach header\n");
	ft_printf("      magic cputype cpusubtype  caps    filetype ncmds "
			"sizeofcmds      flags\n");
	ft_printf(" 0x%08x %7d %10d  0x%02x  %10u %5u %10u 0x%08x\n",
			(unsigned int)mh->magic, mh->cputype,
			mh->cpusubtype & ~CPU_SUBTYPE_MASK,
			(unsigned int)((mh->cpusubtype & CPU_SUBTYPE_MASK) >> 24),
			mh->filetype, mh->ncmds, mh->sizeofcmds,
			(unsigned int)mh->flags);
}

void		print_mach_header_64(struct mach_header_64 *mh64)
{
	ft_printf("Mach header\n");
	ft_printf("      magic cputype cpusubtype  caps    filetype ncmds "
			"sizeofcmds      flags\n");
	ft_printf(" 0x%08x %7d %10d  0x%02x  %10u %5u %10u 0x%08x\n",
			(unsigned int)mh64->magic, mh64->cputype,
			mh64->cpusubtype & ~CPU_SUBTYPE_MASK,
			(unsigned int)((mh64->cpusubtype & CPU_SUBTYPE_MASK) >> 24),
			mh64->filetype, mh64->ncmds, mh64->sizeofcmds,
			(unsigned int)mh64->flags);
}
