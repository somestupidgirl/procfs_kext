/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_next_line.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/29 13:02:20 by mmonier           #+#    #+#             */
/*   Updated: 2019/09/12 23:27:23 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"
#include <unistd.h>
#include <stdlib.h>

int			cnt_line(char *s)
{
	int		i;

	i = 0;
	while (s[i] && s[i] != '\n')
		i++;
	return (i);
}

int			ft_read_fill(char **s, int fd)
{
	int		ret;
	char	buff[12 + 1];

	while ((ret = read(fd, &buff, 12)) > 0)
	{
		buff[ret] = '\0';
		if (!(*s))
		{
			if (!(*s = ft_strdup(buff)))
				return (-1);
		}
		else
		{
			if (!(*s = ft_strfjoin(*s, buff)))
				return (-1);
		}
		if (ft_strchr(*s, '\n'))
			return (ret);
	}
	return (ret);
}

int			main_loop(char **s, char **line, char **tmp)
{
	int i;

	i = cnt_line(*s);
	if ((*s)[0] == '\0')
		ft_strdel(&(*s));
	else
	{
		*line = ft_strsub(*s, 0, i);
		if ((*s)[i] == '\0' || (*s)[i + 1] == '\0')
		{
			free(*s);
			*s = NULL;
			return (1);
		}
		*tmp = ft_strdup(&(*s)[i + 1]);
		free(*s);
		*s = *tmp;
	}
	return (1);
}

int			get_next_line(const int fd, char **line)
{
	int			i;
	int			ret;
	static char	*s;
	char		*tmp;
	char		buff;

	if (fd < 0 || line == NULL || read(fd, &buff, 0) < 0)
		return (-1);
	*line = NULL;
	if ((i = ft_read_fill(&s, fd)) < 0)
	{
		if (s)
			free(s);
		s = NULL;
		return (i);
	}
	if (i == 0 && s == NULL)
		return (0);
	ret = main_loop(&s, line, &tmp);
	return (ret);
}
