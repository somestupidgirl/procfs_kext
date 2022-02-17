/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_mach_o.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/28 18:50:16 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/09 21:57:01 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

int				is_macho(uint32_t magic)
{
	return (magic == MH_MAGIC || magic == MH_CIGAM ||
			magic == MH_MAGIC_64 || magic == MH_CIGAM_64);
}

int				macho_64(uint32_t magic)
{
	return (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);
}

int				integrity_leave(void *file, unsigned long size, int err)
{
	if (file)
		munmap(file, size);
	return (err);
}

static int		ft_mach_o_64(t_ofile *ofile, void *addr,
						uint32_t magic, enum e_byte_sex e)
{
	ofile->file_type = OFILE_Mach_O;
	ofile->object_addr = addr;
	ofile->object_size = ofile->file_size;
	if (magic == MH_MAGIC_64)
		ofile->object_byte_sex = e;
	else
		ofile->object_byte_sex = (e == BIG_ENDIAN_BYTE_SEX)
									? LITTLE_ENDIAN_BYTE_SEX
									: BIG_ENDIAN_BYTE_SEX;
	ofile->mh64 = (struct mach_header_64 *)addr;
	ofile->load_commands = (struct load_command *)
	(addr + sizeof(struct mach_header_64));
	return (0);
}

/*
** Store informations if current file is MH_MAGIC or MH_MAGIC_64
** Store object_address, object_size, filetype, fat_header,
** fat_arch, mach_header, load_commands
** Set the byte_sex of the current object.
*/

int				ft_mach_o(t_ofile *ofile, void *addr,
			uint32_t magic, enum e_byte_sex e)
{
	if (ofile->file_size >= sizeof(struct mach_header) &&
		(magic == MH_MAGIC || magic == swap_uint32(MH_MAGIC)))
	{
		ofile->file_type = OFILE_Mach_O;
		ofile->object_addr = addr;
		ofile->object_size = ofile->file_size;
		if (magic == MH_MAGIC)
			ofile->object_byte_sex = e;
		else
			ofile->object_byte_sex = (e == BIG_ENDIAN_BYTE_SEX)
										? LITTLE_ENDIAN_BYTE_SEX
										: BIG_ENDIAN_BYTE_SEX;
		ofile->mh = (struct mach_header *)addr;
		ofile->load_commands = (struct load_command *)
		(addr + sizeof(struct mach_header));
	}
	else if (ofile->file_size >= sizeof(struct mach_header_64) &&
			(magic == MH_MAGIC_64 || magic == swap_uint32(MH_MAGIC_64)))
		ft_mach_o_64(ofile, addr, magic, e);
	return (0);
}
