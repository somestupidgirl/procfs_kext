/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   processor.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/08 17:43:05 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:13:28 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"
#include "ft_otool.h"
#include <mach-o/ranlib.h>

static void	init_data(t_data *data, t_ofile *o)
{
	ft_memset(data, 0, sizeof(t_data));
	data->addr = o->object_addr;
	data->size = o->object_size;
	data->mh_magic = o->mh ? o->mh->magic : o->mh64->magic;
	data->mh_cputype = o->mh ? o->mh->cputype : o->mh64->cputype;
	data->mh_cpusubtype = o->mh ? o->mh->cpusubtype : o->mh64->cpusubtype;
	data->mh_filetype = o->mh ? o->mh->filetype : o->mh64->filetype;
	data->mh_ncmds = o->mh ? o->mh->ncmds : o->mh64->ncmds;
	data->mh_sizeofcmds = o->mh ? o->mh->sizeofcmds : o->mh64->sizeofcmds;
	data->sizeof_mach_header = o->mh
		? sizeof(struct mach_header) : sizeof(struct mach_header_64);
}

static void	process_set(t_ofile *of, t_flags *f)
{
	t_data				data;

	init_data(&data, of);
	if (f->h || f->l)
		(of->mh) ? print_mach_header(of->mh) : print_mach_header_64(of->mh64);
	if (f->l)
		print_loadcmds(of, &data);
	if (f->ll || f->dd)
		print_libraries(of->load_commands, &data);
	if (f->t)
	{
		get_sect_info(of, &data);
		print_text(&data);
	}
}

/*
** Processor otool.
** Print data in ofile struct.
** Apply is given flag by (void *)cookie.
*/

void		processor(t_ofile *of, char *arch_name, void *cookie)
{
	if (of->member_ar_hdr != NULL &&
		(ft_strncmp(of->member_name, SYMDEF, sizeof(SYMDEF) - 1) == 0 ||
		ft_strncmp(of->member_name, "/ ", sizeof("/ ") - 1) == 0))
		return ;
	if (of->member_ar_hdr && ft_strncmp(of->member_name,
				"// ", sizeof("// ") - 1) == 0)
		return ;
	if (((t_flags *)cookie)->object_process == 0)
		return ;
	print_header(of);
	if (of->mh == NULL && of->mh64 == NULL)
		return ;
	process_set(of, cookie);
	(void)arch_name;
}
