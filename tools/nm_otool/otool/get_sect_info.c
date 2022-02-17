/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_sect_info.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/10 18:15:58 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:11:03 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_otool.h"

static void	lc_seg_32(t_data *data, struct load_command *lc, int *found)
{
	struct segment_command	sg;
	struct section			s;
	char					*p;
	uint32_t				i;

	ft_memcpy((char *)&sg, (char *)lc, sizeof(struct segment_command));
	if ((data->mh_filetype = MH_OBJECT && sg.segname[0] == '\0') ||
			ft_strncmp(sg.segname, SEG_TEXT, sizeof(sg.segname)) == 0)
		data->seg_addr = sg.vmaddr;
	p = (char *)lc + sizeof(struct segment_command);
	i = -1;
	while (*found == 0 && ++i < sg.nsects)
	{
		ft_memcpy((char *)&s, p, sizeof(struct section));
		if (ft_strncmp(s.sectname, SECT_TEXT, sizeof(s.sectname)) == 0 &&
				ft_strncmp(s.segname, SEG_TEXT, sizeof(s.segname)) == 0)
		{
			*found = 1;
			data->cmd = LC_SEGMENT;
		}
		p += sizeof(struct segment_command);
	}
}

static void	lc_seg_64(t_data *data, struct load_command *lc, int *found)
{
	struct segment_command_64	sg;
	struct section_64			s;
	char						*p;
	uint32_t					i;

	ft_memcpy((char *)&sg, (char *)lc, sizeof(struct segment_command_64));
	if ((data->mh_filetype == MH_OBJECT && sg.segname[0] == '\0') ||
			ft_strncmp(sg.segname, SEG_TEXT, sizeof(sg.segname)) == 0)
		data->seg_addr = sg.vmaddr;
	p = (char *)lc + sizeof(struct segment_command_64);
	i = -1;
	data->sg64 = &sg;
	while (*found == 0 && ++i < sg.nsects)
	{
		ft_memcpy((char *)&s, p, sizeof(struct section_64));
		data->s64 = &s;
		if (ft_strncmp(s.sectname, SECT_TEXT, sizeof(s.sectname)) == 0 &&
				ft_strncmp(s.segname, SEG_TEXT, sizeof(s.segname)) == 0)
		{
			*found = 1;
			data->cmd = LC_SEGMENT_64;
		}
		p += sizeof(struct segment_command_64);
	}
}

void		treat_file(t_ofile *of, t_data *data)
{
	if (data->cmd == LC_SEGMENT)
		;
	else
	{
		(void)of;
		data->sect = of->object_addr + data->s64->offset;
		data->sect_size = data->s64->size;
		data->sect_relocs = (struct relocation_info *)
			(of->object_addr + data->s64->reloff);
		data->sect_nrelocs = data->s64->nreloc;
		data->sect_addr = data->s64->addr;
		data->sect_flags = data->s64->flags;
	}
}

void		get_sect_info(t_ofile *of, t_data *data)
{
	struct load_command	*lc;
	struct load_command	l;
	int					found;
	uint32_t			i;

	i = -1;
	found = 0;
	lc = of->load_commands;
	data->sect_size = 0;
	data->sect_addr = 0;
	data->sect_relocs = NULL;
	data->sect_nrelocs = 0;
	data->sect_flags = 0;
	while (found == 0 && ++i < data->mh_ncmds)
	{
		ft_memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
		if (l.cmd == LC_SEGMENT)
			lc_seg_32(data, lc, &found);
		else if (l.cmd == LC_SEGMENT_64)
			lc_seg_64(data, lc, &found);
		lc = (struct load_command *)((char *)lc + l.cmdsize);
	}
	if (found == 0)
		return ;
	treat_file(of, data);
}
