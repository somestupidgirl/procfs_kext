/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ofile_unmap.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/28 19:34:12 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:44:22 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ofile.h"

void		ofile_unmap(t_prg *prg, t_ofile *of)
{
	(void)prg;
	if (of->file_addr)
		munmap(of->file_addr, of->file_size);
	if (of->file_name)
		free(of->file_name);
	ft_memset(of, '\0', sizeof(t_ofile));
}
