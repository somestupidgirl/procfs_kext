/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ofile_member.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/28 23:45:51 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/11 18:54:53 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

static int		ofile_ext_format(t_ofile *ofile,
		void *addr, unsigned long offset)
{
	struct ar_hdr	*ar_hdr;
	unsigned long	ar_name_size;

	ar_hdr = (struct ar_hdr *)(addr + offset);
	ar_name_size = 0;
	offset += sizeof(struct ar_hdr);
	ofile->member_offset = offset;
	ofile->member_addr = addr + offset;
	ofile->member_size = strtoul(ar_hdr->ar_size, NULL, 10);
	ofile->member_ar_hdr = ar_hdr;
	ofile->member_name = ar_hdr->ar_name;
	if (strncmp(ofile->member_name, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0)
	{
		ofile->member_name = ar_hdr->ar_name + sizeof(struct ar_hdr);
		ar_name_size = ft_strtoul(ar_hdr->ar_name
				+ sizeof(AR_EFMT1) - 1, NULL, 10);
		ofile->member_name_size = ar_name_size;
		ofile->member_offset += ar_name_size;
		ofile->member_addr += ar_name_size;
		ofile->member_size -= ar_name_size;
	}
	else
		ofile->member_name_size = size_ar_name(ar_hdr);
	return (0);
}

static int		ofile_set_fat(t_ofile *ofile)
{
	uint32_t		magic;

	ft_memcpy(&magic, ofile->member_addr, sizeof(unsigned long));
	if (magic == FAT_MAGIC || magic == swap_long(FAT_MAGIC))
	{
		ofile->member_type = OFILE_FAT;
		ofile->fat_header = (struct fat_header *)(ofile->member_addr);
		if (magic == swap_uint32(FAT_MAGIC))
			swap_fat_header(ofile->fat_header);
		ofile->fat_archs = (struct fat_arch *)
			(ofile->member_addr + sizeof(struct fat_header));
	}
	return (0);
}

static int		ofile_set_mach_o(t_ofile *of,
		unsigned long offset, unsigned long size)
{
	unsigned long	magic;
	enum e_byte_sex	e;

	e = get_host_byte_sex();
	ft_memcpy(&magic, of->member_addr, sizeof(unsigned long));
	if (size - (offset + of->member_name_size) >= sizeof(struct mach_header)
			&& (magic == MH_MAGIC || magic == swap_uint32(MH_MAGIC)))
	{
		of->member_type = OFILE_Mach_O;
		of->object_addr = of->member_addr;
		of->object_size = of->member_size;
		if (magic == MH_MAGIC)
			of->object_byte_sex = e;
		else
			of->object_byte_sex = (e == BIG_ENDIAN_BYTE_SEX)
				? LITTLE_ENDIAN_BYTE_SEX : BIG_ENDIAN_BYTE_SEX;
		of->mh = (struct mach_header *)of->object_addr;
		of->load_commands = (struct load_command *)
			(of->object_addr + sizeof(struct mach_header));
	}
	return (0);
}

static int		ofile_set_mach_o64(t_ofile *of,
		unsigned long offset, unsigned long size)
{
	unsigned int	magic;
	enum e_byte_sex	e;

	e = get_host_byte_sex();
	ft_memcpy(&magic, of->member_addr, sizeof(unsigned int));
	if (size - (offset + of->member_name_size) >= sizeof(struct mach_header)
			&& (magic == MH_MAGIC_64 || magic == swap_uint32(MH_MAGIC_64)))
	{
		of->member_type = OFILE_Mach_O;
		of->object_addr = of->member_addr;
		of->object_size = of->member_size;
		if (magic == MH_MAGIC)
			of->object_byte_sex = e;
		else
			of->object_byte_sex = (e == BIG_ENDIAN_BYTE_SEX)
				? LITTLE_ENDIAN_BYTE_SEX : BIG_ENDIAN_BYTE_SEX;
		of->mh64 = (struct mach_header_64 *)of->object_addr;
		of->load_commands = (struct load_command *)
			(of->object_addr + sizeof(struct mach_header_64));
	}
	return (0);
}

/*
** ofile_next_member() set up the ofile structure (the member_* fields and
** the object file fields if the next member is an object file) for the next
** member.
*/

int				ofile_next_member(t_ofile *ofile)
{
	void			*addr;
	unsigned long	size;
	unsigned long	offset;

	if (ofile->file_type == OFILE_ARCHIVE)
	{
		addr = ofile->file_addr;
		size = ofile->file_size;
	}
	else
		return (1);
	offset = ofile->member_offset + ft_round(ofile->member_size, sizeof(short));
	if (offset == size)
		return (1);
	ofile_ext_format(ofile, addr, offset);
	offset += sizeof(struct ar_hdr);
	ofile_object_clear(ofile);
	if (ofile->member_size > sizeof(unsigned long))
	{
		ofile_set_fat(ofile);
		ofile_set_mach_o(ofile, offset, size);
		ofile_set_mach_o64(ofile, offset, size);
	}
	return (0);
}
