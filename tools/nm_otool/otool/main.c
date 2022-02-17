/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/08 17:30:23 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:22:17 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_printf.h"
#include "ft_otool.h"

static int	ft_error(char *s, int err)
{
	static char *tab[] = {NULL, "malloc error", "open error", S_STAT,
		S_MMAP, "write error", "archive err", "archive empty error",
		S_MMAP, S_MH, S_SIZECMDS, S_LC, S_ALIGN, S_CMDSIZE, S_FILEOFF,
		S_FILEOFF_SIZE, S_SECTION, S_SEC_OFF, S_RELOFF};

	if (err == -1)
	{
		ft_dprintf(2, "otool <opts> <object file>\n");
		return (1);
	}
	ft_printf("ft_nm: ");
	ft_printf(tab[err], s);
	ft_printf("\n");
	return (err);
}

int			main(int ac, char **av)
{
	t_prg		prg;
	t_flags		f;
	int			err;
	size_t		i;

	ft_memset(&prg, 0, sizeof(prg));
	i = parse_flag(&f, av, ac, 1);
	prg.pnam = av[0];
	prg.target = av[i] ? av[i] : NULL;
	prg.mul = av[i] && av[i + 1] ? 1 : 0;
	if (!prg.target)
		return (ft_error(NULL, -1));
	prg.proc = &processor;
	while (prg.target)
	{
		if (prg.mul)
			ft_printf("%s:\n", prg.target);
		err = ofile_create(&prg, &f);
		if (err)
			err = ft_error(prg.target, err);
		i++;
		prg.target = av[i];
	}
	return (err);
}
