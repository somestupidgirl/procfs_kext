/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strstr.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/09 11:47:16 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/12 12:33:26 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

char	*ft_strstr(const char *str, const char *needle)
{
	int i;
	int k;

	i = 0;
	if (!(*needle))
		return ((char *)str);
	while (str[i])
	{
		k = 0;
		while (str[i + k] == needle[k])
		{
			if (needle[k + 1] == '\0')
				return ((char*)str + i);
			k++;
		}
		i++;
	}
	return (0);
}
