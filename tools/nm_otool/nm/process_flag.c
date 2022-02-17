/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   process_flag.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/11/19 17:37:48 by dbaffier          #+#    #+#             */
/*   Updated: 2019/12/12 23:26:59 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_nm.h"

static void		manage_sect_32(t_process_flg *f,
		struct load_command *lc, size_t *k)
{
	struct segment_command	*sc;
	struct section			*s;
	size_t					j;

	j = 0;
	sc = (struct segment_command *)lc;
	s = (struct section *)((char *)sc + sizeof(struct segment_command));
	while (j < sc->nsects)
	{
		if (!ft_strcmp((s + j)->sectname, SECT_TEXT) &&
				!ft_strcmp((s + j)->segname, SEG_TEXT))
			f->text_nsect = *k + 1;
		else if (!ft_strcmp((s + j)->sectname, SECT_DATA) &&
				!ft_strcmp((s + j)->segname, SEG_DATA))
			f->data_nsect = *k + 1;
		else if (!ft_strcmp((s + j)->sectname, SECT_BSS) &&
				!ft_strcmp((s + j)->segname, SEG_DATA))
			f->bss_nsect = *k + 1;
		j++;
		f->sections[(*k)++] = s + j;
	}
}

/*
** Directly following a segment_command data structure
** is an array of section data structures, with the exact count determined
** by the nsects field of the segment_command
*/

static void		manage_sect_64(t_process_flg *f,
		struct load_command *lc, size_t *k)
{
	struct segment_command_64	*sc;
	struct section_64			*s;
	size_t						j;

	j = 0;
	sc = (struct segment_command_64 *)lc;
	s = (struct section_64 *)((char *)sc + sizeof(struct segment_command_64));
	while (j < sc->nsects)
	{
		if (!ft_strcmp((s + j)->sectname, SECT_TEXT) &&
				!ft_strcmp((s + j)->segname, SEG_TEXT))
			f->text_nsect = *k + 1;
		else if (!ft_strcmp((s + j)->sectname, SECT_DATA) &&
				!ft_strcmp((s + j)->segname, SEG_DATA))
			f->data_nsect = *k + 1;
		else if (!ft_strcmp((s + j)->sectname, SECT_BSS) &&
				!ft_strcmp((s + j)->segname, SEG_DATA))
			f->bss_nsect = *k + 1;
		j++;
		f->sections64[(*k)++] = s + j;
	}
}

/*
** Mallocate space for the number of sections in.
** Store section in the mallocate array.
*/

void			process_flg_sect(t_nm *nm, t_ofile *of,
		t_process_flg *f, struct load_command *lc)
{
	size_t		i;
	size_t		k;

	i = 0;
	k = 0;
	if (of->mh)
		f->sections = (struct section **)
			malloc(sizeof(struct section *) * f->nsects);
	else
		f->sections64 = (struct section_64 **)
			malloc(sizeof(struct section_64 *) * f->nsects);
	while (i < nm->ncmds)
	{
		if (lc->cmd == LC_SEGMENT)
			manage_sect_32(f, lc, &k);
		else if (lc->cmd == LC_SEGMENT_64)
			manage_sect_64(f, lc, &k);
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
		i++;
	}
}
