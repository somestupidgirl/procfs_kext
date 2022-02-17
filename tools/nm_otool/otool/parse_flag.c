/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parse_flag.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/08 21:31:06 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 18:26:13 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_otool.h"

int		ft_err(char *err)
{
	ft_dprintf(2, "error: ./ft_otool %s\n", err);
	ft_dprintf(2,
			"Usage: %s [-fhlLDtd] "
			"<object file> ...\n", "./ft_otool");
	ft_dprintf(2, "\t-f print the fat headers\n");
	ft_dprintf(2, "\t-h print the mach header\n");
	ft_dprintf(2, "\t-l print the load commands\n");
	ft_dprintf(2, "\t-L print shared libraries used\n");
	ft_dprintf(2, "\t-D print shared library id name\n");
	ft_dprintf(2, "\t-t print the text section\n");
	ft_dprintf(2, "\t-d print the data section\n");
	exit(1);
}

int		keep(char c, t_flags *f)
{
	if (c == 'l')
		f->l = 1;
	else if (c == 'h')
		f->h = 1;
	else if (c == 'L')
		f->ll = 1;
	else if (c == 'D')
		f->dd = 1;
	else if (c == 't')
		f->t = 1;
	else if (c == 'd')
		f->d = 1;
	else
		return (0);
	f->object_process = 1;
	return (1);
}

int		set_flag(t_flags *f, char *av, int j)
{
	while (av[j])
	{
		if (av[j] == 'f')
			f->f = 1;
		else if (av[j] == 'S')
			f->ss = 1;
		else
		{
			if (keep(av[j], f) == 0)
				return (ft_err("invalid argument"));
		}
		j++;
	}
	return (1);
}

int		parse_flag(t_flags *f, char **av, int ac, int i)
{
	int		j;

	j = 0;
	ft_memset(f, 0, sizeof(t_flags));
	if (!av[i])
		return (i);
	while (i < ac)
	{
		if (av[i][0] == '-')
		{
			if (av[i][1] == '\0' || av[i][1] == '-')
				return (i + 1);
			else
				set_flag(f, av[i], 1);
		}
		else
			break ;
		i++;
	}
	return (i);
}
