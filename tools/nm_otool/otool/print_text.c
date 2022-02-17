/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   print_text.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/11 17:09:29 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:23:38 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_otool.h"

static uint32_t		print_text_sup(uint32_t j,
		uint32_t i, char *sect, t_data *data)
{
	unsigned char	byte_word;

	while (j < 16 * sizeof(char) && i + j < data->sect_size)
	{
		byte_word = *(sect + i + j);
		ft_printf("%02x ", (unsigned int)byte_word);
		j += sizeof(char);
	}
	ft_printf("\n");
	return (j);
}

void				print_text(t_data *data)
{
	uint32_t		i;
	uint32_t		j;
	char			*sect;

	i = 0;
	ft_printf("Contents of (%.16s,%.16s) section\n", SEG_TEXT, SECT_TEXT);
	sect = data->sect;
	if (data->mh_cputype == CPU_TYPE_I386 ||
			data->mh_cputype == CPU_TYPE_X86_64)
	{
		while (i < data->sect_size)
		{
			(data->mh_cputype & CPU_ARCH_ABI64)
				? ft_printf("%016llx\t", data->sect_addr)
				: ft_printf("%08x\t", (uint32_t)data->sect_addr);
			j = print_text_sup(0, i, sect, data);
			i += j;
			data->sect_addr += j;
		}
	}
}
