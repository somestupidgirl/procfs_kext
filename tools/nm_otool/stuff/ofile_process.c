/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ofile_process.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/24 23:28:11 by dbaffier          #+#    #+#             */
/*   Updated: 2020/07/29 16:05:32 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

/*
** Check for magic number and save is byte_sex (byte Sign EXtend)
** Process the format associated with magic number.
** FAT_FILE, MACH-O, ARCHIVE
** If FAT_FILE, process only x86_64 and jump to the
** beginning of data for this CPU.
*/

static int	ofile_type(t_ofile *of, uint32_t size, void *addr)
{
	uint32_t		magic;
	enum e_byte_sex	host_byte_sex;

	magic = 0;
	if (size >= sizeof(unsigned long))
		magic = *((unsigned long *)addr);
	host_byte_sex = get_host_byte_sex();
	addr += ft_fat(of, addr, magic, host_byte_sex);
	if (size >= sizeof(unsigned long))
		magic = *((unsigned long *)addr);
	ft_mach_o(of, addr, magic, host_byte_sex);
	ft_archive(of, addr, magic, host_byte_sex);
	return (0);
}

/*
** Map the specified filename in memory to read it.
** Call ofile_type to process the corresponding type.
*/

static int	ofile_map(t_prg *nm_t, t_ofile *of)
{
	void		*addr;
	int			fd;
	struct stat	sb;

	if ((fd = open(nm_t->target, O_RDONLY)) == -1)
		return (ERR_OPEN);
	if (fstat(fd, &sb) == -1)
		return (ERR_STAT);
	of->file_size = sb.st_size;
	addr = mmap(NULL, sb.st_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
		return (ERR_MAP);
	of->file_addr = addr;
	close(fd);
	if ((of->file_name = ft_strdup(nm_t->target)) == NULL)
		return (ERR_MALLOC);
	ofile_type(of, sb.st_size, addr);
	return (0);
}

/*
** ofile_create() processes the specified filename and call the
** routine proc on the ofiles in it.
** For each ofile that is to be processed, the routine processor
** is called with the corresponding
** ofile struct and cookie correspond to the given struct of flag.
*/

int			ofile_create(t_prg *nm_t, void *cookie)
{
	int		ret;
	t_ofile	of;

	ft_memset(&of, 0, sizeof(t_ofile));
	if (!ft_strcmp(nm_t->pnam, "./ft_otool") && *((int *)cookie) & 0x1)
		of.prog = 1;
	if ((ret = mach_o_integrity(nm_t->target)) > 0)
		return (ret);
	if ((ret = ofile_map(nm_t, &of)) > 0)
		return (ret);
	if (of.file_type == OFILE_ARCHIVE)
		process_archive(nm_t, &of, cookie);
	else if (of.file_type == OFILE_Mach_O)
		nm_t->proc(&of, NULL, cookie);
	else
		ft_dprintf(2, "%s is not an object file\n", of.file_name);
	ofile_unmap(nm_t, &of);
	return (0);
}
