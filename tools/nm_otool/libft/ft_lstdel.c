/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_lstdel.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/23 12:36:51 by mmonier           #+#    #+#             */
/*   Updated: 2018/11/23 12:42:49 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	ft_lstdel(t_list **alst, void (*del)(void*, size_t))
{
	t_list	*begin;
	t_list	*list;

	list = *alst;
	while (list)
	{
		begin = list->next;
		del(list, list->content_size);
		list = begin;
	}
	*alst = NULL;
}
