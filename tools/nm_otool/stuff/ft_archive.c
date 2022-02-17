/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_archive.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/28 18:58:20 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:15:29 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

int		ft_archive(t_ofile *of, void *addr, uint32_t magic, enum e_byte_sex e)
{
	(void)e;
	(void)magic;
	if (of->file_size >= SARMAG && strncmp(addr, ARMAG, SARMAG) == 0)
	{
		of->file_type = OFILE_ARCHIVE;
		of->object_addr = addr;
	}
	return (0);
}

/*
** Process every member if current object is an archive
** Call associated routine for every member.
*/

int		process_archive(t_prg *prg, t_ofile *of, void *cookie)
{
	if (ofile_first_member(of) == 0)
	{
		if (!ft_strcmp(prg->pnam, "./ft_otool"))
			ft_printf("Archive : %s\n", of->file_name);
		prg->proc(of, NULL, cookie);
	}
	while (ofile_next_member(of) == 0)
		prg->proc(of, NULL, cookie);
	return (1);
}
