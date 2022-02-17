/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   print_libraries.c                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/09 22:05:12 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:09:47 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_otool.h"

static void	print_extend(char *p, struct dylib_command *dl)
{
	ft_printf("\t%s (compatibility version %u.%u.%u, "
			"current version %u.%u.%u)\n", p,
			dl->dylib.compatibility_version >> 16,
			(dl->dylib.compatibility_version >> 8) & 0xff,
			dl->dylib.compatibility_version & 0xff,
			dl->dylib.current_version >> 16,
			(dl->dylib.current_version >> 8) & 0xff,
			dl->dylib.current_version & 0xff);
}

void		print_libraries(struct load_command *load_c, t_data *data)
{
	uint32_t				i;
	struct load_command		*lc;
	struct load_command		l;
	struct dylib_command	dl;
	char					*p;

	i = -1;
	lc = load_c;
	while (++i < data->mh_ncmds)
	{
		ft_memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
		if (l.cmd == LC_ID_DYLIB)
		{
			ft_memcpy((char *)&dl, (char *)lc, sizeof(struct dylib_command));
			if (dl.dylib.name.offset < dl.cmdsize)
			{
				p = (char *)lc + dl.dylib.name.offset;
				print_extend(p, &dl);
			}
		}
		lc = (struct load_command *)((char *)lc + l.cmdsize);
	}
}
