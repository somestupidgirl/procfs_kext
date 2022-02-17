/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   print_lc_seg32.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/09 18:38:41 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:20:57 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_otool.h"

static void		print_segment_command(struct segment_command *sg)
{
	ft_printf("      cmd LC_SEGMENT\n");
	ft_printf("  cmdsize %u\n", sg->cmdsize);
	ft_printf("  segname %.16s\n", sg->segname);
	ft_printf("   vmaddr 0x%08x\n", (uint32_t)sg->vmaddr);
	ft_printf("   vmsize 0x%08x\n", (uint32_t)sg->vmsize);
	ft_printf("  fileoff %llu\n", sg->fileoff);
	ft_printf(" filesize %llu\n", sg->filesize);
	ft_printf("  maxprot 0x%08x\n", (unsigned int)sg->maxprot);
	ft_printf(" initprot 0x%08x\n", (unsigned int)sg->initprot);
	ft_printf("   nsects %u\n", sg->nsects);
	ft_printf("    flags 0x%x\n", (unsigned int)sg->flags);
}

static void		print_section(struct section *s)
{
	ft_printf("Section\n");
	ft_printf("  sectname %.16s\n", s->sectname);
	ft_printf("   segname %.16s\n", s->segname);
	ft_printf("      addr 0x%08x\n", (uint32_t)s->addr);
	ft_printf("      size 0x%08x\n", (uint32_t)s->size);
	ft_printf("    offset %u\n", s->offset);
	ft_printf("     align 2^%u (%d)\n", s->align, 1 << s->align);
	ft_printf("    reloff %u\n", s->reloff);
	ft_printf("    nreloc %u\n", s->nreloc);
	ft_printf("     flags 0x%08x\n", (unsigned int)s->flags);
	ft_printf(" reserved1 %u\n", s->reserved1);
	ft_printf(" reserved2 %u\n", s->reserved2);
}

void			print_lc_segment32(t_ofile *of,
		t_data *data, struct load_command *lc)
{
	uint32_t				j;
	char					*p;
	struct segment_command	sg;
	struct section			s;

	(void)of;
	(void)data;
	ft_memcpy((char *)&sg, (char *)lc, sizeof(struct segment_command));
	print_segment_command(&sg);
	p = (char *)lc + sizeof(struct segment_command);
	j = -1;
	while (++j < sg.nsects)
	{
		ft_memcpy((char *)&s, p, sizeof(struct section));
		print_section(&s);
		p += sizeof(struct section);
	}
}
