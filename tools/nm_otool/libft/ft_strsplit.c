/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strsplit.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/23 12:41:27 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/23 12:53:43 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static int			ft_count_words(const char *s, char c)
{
	int		word;
	int		i;

	i = 0;
	word = 0;
	if (!s)
		return (0);
	while (s[i])
	{
		if (s[i] == c && s[i + 1] != c)
			word++;
		i++;
	}
	if (s[0] != '\0')
		word++;
	return (word);
}

char				*ft_mallocd2(const char *s, char c, int *i)
{
	char	*d;
	int		k;

	if (!(d = (char *)malloc(sizeof(d) * (ft_strlen(s)))))
		return (NULL);
	k = 0;
	while (s[*i] != c && s[*i])
	{
		d[k] = s[*i];
		k++;
		(*i)++;
	}
	d[k] = '\0';
	while (s[*i] == c && s[*i])
		(*i)++;
	return (d);
}

char				**ft_strsplit(const char *s, char c)
{
	int		i;
	int		j;
	int		nb_word;
	char	**tab;

	i = 0;
	j = 0;
	if (!s)
		return (NULL);
	nb_word = ft_count_words(s, c);
	if (!(tab = (char **)malloc(sizeof(s) * (ft_count_words(s, c) + 2))))
		return (NULL);
	while (s[i] == c && s[i])
		i++;
	while (j < nb_word && s[i])
	{
		tab[j] = ft_mallocd2(s, c, &i);
		j++;
	}
	tab[j] = NULL;
	return (tab);
}
