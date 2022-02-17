/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/24 00:35:15 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:50:38 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"
#include "ft_printf.h"
#include "ft_ofile.h"
#include "ft_nm.h"

static int	ft_error(char *s, int err)
{
	static char *tab[] = {NULL, "malloc error", "open error", S_STAT,
		S_MMAP, "write error", "archive err", "archive empty error",
		S_MMAP, S_MH, S_SIZECMDS, S_LC, S_ALIGN, S_CMDSIZE, S_FILEOFF,
		S_FILEOFF_SIZE, S_SECTION, S_SEC_OFF, S_RELOFF};

	ft_printf("ft_nm: ");
	ft_printf(tab[err], s);
	ft_printf("\n");
	return (err);
}

/*
** Main entry of nm
** Initialize t_prg, parse flag in t_flags structure (see ft_nm.h)
** All options parsed, if no filename specified, do a.out
** Loop over filename if given,
** Create an ofile structure containing informations about filename
** Apply flag and execute is given routine (&nm)
*/

int			main(int ac, char **av)
{
	t_prg	prg;
	t_flags	f;
	int		err;
	size_t	i;

	ft_memset(&prg, 0, sizeof(prg));
	prg.pnam = av[0];
	i = parse_flag(&f, av, ac, 1);
	prg.target = av[i] ? av[i] : "a.out";
	prg.proc = &nm;
	prg.mul = av[i] && av[i + 1] ? 1 : 0;
	ft_printf("%s", prg.mul ? "\n" : "");
	while (prg.target)
	{
		if (prg.mul)
			ft_printf("%s:\n", prg.target);
		err = ofile_create(&prg, &f);
		if (prg.mul && av[i + 1])
			ft_printf("\n");
		i++;
		if (err)
			err = ft_error(prg.target, err);
		prg.target = av[i];
	}
	return (err);
}
