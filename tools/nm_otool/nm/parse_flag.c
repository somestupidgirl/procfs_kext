/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parse_flag.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/01 17:21:33 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:03:57 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_nm.h"

int	ft_err(char *err)
{
	if (ft_strcmp("-t", err))
		ft_dprintf(2, "error: ./ft_nm %s\n", err);
	ft_dprintf(2, "usage: ./ft_nm [-agopuUjA] [file ...]\n");
	ft_dprintf(2, ARGS0);
	ft_dprintf(2, ARGS1);
	ft_dprintf(2, ARGS3);
	ft_dprintf(2, ARGS4);
	ft_dprintf(2, ARGS6);
	ft_dprintf(2, ARGS7);
	ft_dprintf(2, ARGS8);
	ft_dprintf(2, ARGS9);
	exit(1);
}

int	keep(char c, t_flags *f)
{
	if (c == 'u')
	{
		if (f->uu == 1)
			return (ft_err("can't specify both -u and -U"));
		f->u = 1;
	}
	else if (c == 'U')
	{
		if (f->u == 1)
			return (ft_err("can't specify both -U and -u"));
		f->uu = 1;
	}
	else if (c == 'j')
		f->j = 1;
	else if (c == 'A')
		f->aa = 1;
	else
		return (0);
	return (1);
}

int	set_flag(t_flags *f, char *av, int j)
{
	while (av[j])
	{
		if (av[j] == 'a')
			f->a = 1;
		else if (av[j] == 'g')
			f->g = 1;
		else if (av[j] == 'o')
			f->o = 1;
		else if (av[j] == 'p')
			f->p = 1;
		else
		{
			if (keep(av[j], f) == 0)
				return (ft_err("invalid argument"));
		}
		j++;
	}
	return (1);
}

int	parse_flag(t_flags *f, char **av, int ac, int i)
{
	int j;

	j = 0;
	ft_memset(f, 0, sizeof(t_flags));
	if (!av[i])
		return (1);
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
