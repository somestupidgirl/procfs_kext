/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ofile_first_member.c                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/07 19:27:13 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/11 18:55:17 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

static void		ofile_set(t_ofile *of)
{
	of->member_offset = 0;
	of->member_addr = NULL;
	of->member_size = 0;
	of->member_ar_hdr = NULL;
	of->member_name = NULL;
	of->member_name_size = 0;
	of->member_type = OFILE_UNKNOWN;
	of->object_addr = NULL;
	of->object_size = 0;
	of->object_byte_sex = UNKNOWN_BYTE_SEX;
	of->mh = NULL;
	of->load_commands = NULL;
}

static void		ofile_ext(t_ofile *of, unsigned long offset, void *addr)
{
	struct ar_hdr	*ar_hdr;
	unsigned long	ar_name_size;

	ar_hdr = (struct ar_hdr *)(addr + offset);
	ar_name_size = 0;
	offset += sizeof(struct ar_hdr);
	of->member_offset = offset;
	of->member_addr = addr + offset;
	of->member_size = strtoul(ar_hdr->ar_size, NULL, 10);
	of->member_ar_hdr = ar_hdr;
	of->member_type = OFILE_UNKNOWN;
	of->member_name = ar_hdr->ar_name;
	if (strncmp(of->member_name, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0)
	{
		of->member_name = ar_hdr->ar_name + sizeof(struct ar_hdr);
		ar_name_size = strtoul(ar_hdr->ar_name + sizeof(AR_EFMT1) - 1,
				NULL, 10);
		of->member_name_size = ar_name_size;
		of->member_offset += ar_name_size;
		of->member_addr += ar_name_size;
		of->member_size -= ar_name_size;
	}
	else
		of->member_name_size = size_ar_name(ar_hdr);
}

static void		ofile_fat_bis(t_ofile *of,
		enum e_byte_sex e, unsigned long magic)
{
	of->member_type = OFILE_Mach_O;
	of->object_addr = of->member_addr;
	of->object_size = of->member_size;
	if (magic == MH_MAGIC)
		of->object_byte_sex = e;
	else
		of->object_byte_sex = (e == BIG_ENDIAN_BYTE_SEX)
			? LITTLE_ENDIAN_BYTE_SEX : BIG_ENDIAN_BYTE_SEX;
	of->mh = (struct mach_header *)(of->object_addr);
	of->load_commands = (struct load_command *)
		(of->object_addr + sizeof(struct mach_header));
}

static void		ofile_fat(t_ofile *of, unsigned long offset,
		unsigned long size, unsigned long ar_name_size)
{
	enum e_byte_sex	e;
	unsigned long	magic;

	e = get_host_byte_sex();
	if (of->member_size > sizeof(unsigned long))
	{
		memcpy(&magic, of->member_addr, sizeof(unsigned long));
		if (magic == FAT_MAGIC || magic == swap_uint32(FAT_MAGIC))
		{
			of->member_type = OFILE_FAT;
			of->fat_header = (struct fat_header *)(of->member_addr);
			if (magic == swap_uint32(FAT_MAGIC))
				swap_fat_header(of->fat_header);
			of->fat_archs = (struct fat_arch *)
				(of->member_addr + sizeof(struct fat_header));
			if (size - (offset + ar_name_size) >= sizeof(struct mach_header) &&
					(magic == MH_MAGIC || magic == swap_uint32(MH_MAGIC)))
				ofile_fat_bis(of, e, magic);
		}
	}
}

/*
** ofile_first_member() set up the ofile structure (the member_* fields and
** the object file fields if the first member is an object file) for the first
** member.
*/

int				ofile_first_member(t_ofile *of)
{
	void			*addr;
	unsigned long	size;
	unsigned long	offset;

	ofile_set(of);
	if (of->file_type == OFILE_ARCHIVE)
	{
		addr = of->file_addr;
		size = of->file_size;
	}
	else
		return (1);
	if (size < SARMAG || strncmp(addr, ARMAG, SARMAG) != 0)
		return (1);
	offset = SARMAG;
	if (offset != size && offset + sizeof(struct ar_hdr) > size)
		return (ERR_ARCHIVE);
	if (size == offset)
		return (ERR_ARCHIVE_EMPTY);
	ofile_ext(of, offset, addr);
	offset += sizeof(struct ar_hdr);
	ofile_fat(of, offset, size, of->member_name_size);
	return (0);
}
