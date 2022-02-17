/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   print_loadcmds.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/08 23:09:44 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:19:32 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_otool.h"

void	print_loadcmds(t_ofile *of, t_data *data)
{
	struct load_command		*lc;
	struct load_command		l;
	uint32_t				i;
	int						swapped;

	swapped = get_host_byte_sex() != of->object_byte_sex;
	lc = of->load_commands;
	i = -1;
	while (++i < data->mh_ncmds)
	{
		ft_printf("Load command %u\n", i);
		ft_memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
		if (l.cmd == LC_SEGMENT)
			print_lc_segment32(of, data, lc);
		else if (l.cmd == LC_SEGMENT_64)
			print_lc_segment64(of, data, lc);
		else if (l.cmd == LC_SYMTAB)
			print_lc_symtab(of, data, lc);
		lc = (struct load_command *)((char *)lc + l.cmdsize);
	}
}
