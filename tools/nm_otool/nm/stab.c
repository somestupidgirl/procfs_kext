/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   stab.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/07 18:28:09 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/07 20:48:14 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_nm.h"
#include <mach-o/stab.h>

static const struct s_stabnames	stabnames[] = {
	{ N_GSYM, "GSYM" },
	{ N_FNAME, "FNAME" },
	{ N_FUN, "FUN" },
	{ N_STSYM, "STSYM" },
	{ N_LCSYM, "LCSYM" },
	{ N_BNSYM, "BNSYM" },
	{ N_OPT, "OPT" },
	{ N_RSYM, "RSYM" },
	{ N_SLINE, "SLINE" },
	{ N_ENSYM, "ENSYM" },
	{ N_SSYM, "SSYM" },
	{ N_SO, "SO" },
	{ N_OSO, "OSO" },
	{ N_LSYM, "LSYM" },
	{ N_BINCL, "BINCL" },
	{ N_SOL, "SOL" },
	{ N_PARAMS, "PARAM" },
	{ N_VERSION, "VERS" },
	{ N_OLEVEL, "OLEV" },
	{ N_PSYM, "PSYM" },
	{ N_EINCL, "EINCL" },
	{ N_ENTRY, "ENTRY" },
	{ N_LBRAC, "LBRAC" },
	{ N_EXCL, "EXCL" },
	{ N_RBRAC, "RBRAC" },
	{ N_BCOMM, "BCOMM" },
	{ N_ECOMM, "ECOMM" },
	{ N_ECOML, "ECOML" },
	{ N_LENG, "LENG" },
	{ N_PC, "PC" },
	{ 0, 0 }};

static char						*stab(unsigned char n_type)
{
	const struct s_stabnames *p;

	p = stabnames;
	while (p->name)
	{
		if (p->n_type == n_type)
			return (p->name);
		p++;
	}
	return (NULL);
}

int								print_stab(t_nm *nm, size_t i)
{
	if (nm->select_sym[i].nl.n_type & N_STAB)
	{
		ft_printf("%016llx", nm->select_sym[i].nl.n_value);
		ft_printf(" - %02x %04x %5.5s ",
			(unsigned int)nm->select_sym[i].nl.n_sect & 0xff,
			(unsigned int)nm->select_sym[i].nl.n_desc & 0xffff,
			stab(nm->select_sym[i].nl.n_type));
		ft_printf("%s\n", nm->select_sym[i].name);
		return (1);
	}
	return (0);
}
