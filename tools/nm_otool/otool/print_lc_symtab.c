/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   print_lc_symtab.c                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/12/09 21:14:58 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/13 00:20:35 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_otool.h"

static void	print_symtab_command(struct symtab_command *st)
{
	ft_printf("     cmd LC_SYMTAB\n");
	ft_printf(" cmdsize %u\n", st->cmdsize);
	ft_printf("  symoff %u\n", st->symoff);
	ft_printf("   nsyms %u\n", st->nsyms);
	ft_printf("  stroff %u\n", st->stroff);
	ft_printf(" strsize %u\n", st->strsize);
}

void		print_lc_symtab(t_ofile *of, t_data *data, struct load_command *lc)
{
	struct symtab_command st;

	ft_memcpy((char *)&st, (char *)lc, sizeof(struct symtab_command));
	print_symtab_command(&st);
	(void)of;
	(void)data;
}
