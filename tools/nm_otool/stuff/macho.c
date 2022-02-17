/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   macho.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/07 20:29:47 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 20:17:42 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

static int	section_check(struct segment_command_64 *sg64, uint64_t big_size,
		unsigned long size, uint32_t shd)
{
	struct section_64	*s64;
	uint32_t			i;

	s64 = (struct section_64 *)((char *)sg64
		+ sizeof(struct segment_command_64));
	i = 0;
	while (i < sg64->nsects)
	{
		if (sg64->fileoff == 0 && s64->offset < shd && s64->size != 0)
			return (ERR_SECTION);
		big_size = s64->offset;
		big_size += s64->size;
		if (big_size > size)
			return (ERR_SEC_OFF);
		if (s64->reloff > size)
			return (ERR_RELOFF);
		i++;
	}
	return (0);
}

static int	segment_check(struct load_command *lc,
		unsigned long size, uint32_t shd)
{
	struct segment_command_64	*sg64;
	uint64_t					big_size;
	int							err;

	if (lc->cmdsize < sizeof(struct segment_command_64))
		return (ERR_CMDSIZE);
	sg64 = (struct segment_command_64 *)lc;
	big_size = sg64->nsects;
	big_size *= sizeof(struct section_64);
	big_size += sizeof(struct segment_command_64);
	if (sg64->cmdsize != big_size)
		return (ERR_CMDSIZE);
	if (sg64->fileoff > size)
		return (ERR_FILEOFF);
	big_size = sg64->fileoff;
	big_size += sg64->filesize;
	if (big_size > size)
		return (ERR_FILEOFF_SIZE);
	err = section_check(sg64, big_size, size, shd);
	return (err ? err : 0);
}

static int	lc_check(void *file, struct mach_header_64 *mh64,
	unsigned long size)
{
	uint32_t			i;
	int					err;
	uint64_t			big_load_end;
	struct load_command	*lc;

	i = 0;
	big_load_end = 0;
	lc = (struct load_command *)(file + sizeof(struct mach_header_64));
	while (i < mh64->ncmds)
	{
		if (big_load_end + sizeof(struct load_command) > mh64->sizeofcmds)
			return (ERR_LC);
		if (lc->cmdsize % 8 != 0)
			return (ERR_ALIGN);
		big_load_end += lc->cmdsize;
		if (big_load_end > size || lc->cmdsize == 0)
			return (ERR_CMDSIZE);
		if (lc->cmd == LC_SEGMENT_64)
			if ((err = segment_check(lc, size, mh64->sizeofcmds
			+ sizeof(struct mach_header_64))) > 0)
				return (err);
		i++;
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return (0);
}

int			mach_o_integrity(char *path)
{
	void						*file;
	struct stat					s;
	struct mach_header_64		*mh64;
	int							fd;
	int							err;

	if (stat(path, &s) == -1)
		return (integrity_leave(NULL, 0, ERR_STAT));
	if ((fd = open(path, O_RDONLY)) < 0)
		return (integrity_leave(NULL, 0, ERR_OPEN));
	file = mmap(NULL, s.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	close(fd);
	if (file == MAP_FAILED)
		return (integrity_leave(NULL, 0, ERR_MMAP));
	if (!is_macho(*(unsigned int *)file) || macho_64(*(unsigned int *)file))
		return (integrity_leave(file, s.st_size, 0));
	if (!(mh64 = (struct mach_header_64 *)file))
		return (integrity_leave(file, s.st_size, ERR_MH));
	if (mh64->sizeofcmds + sizeof(struct mach_header_64)
		> (unsigned long)s.st_size)
		return (integrity_leave(file, s.st_size, ERR_SIZECMDS));
	if ((err = lc_check(file, mh64, s.st_size)) > 0)
		return (integrity_leave(file, s.st_size, err));
	munmap(file, s.st_size);
	return (0);
}
