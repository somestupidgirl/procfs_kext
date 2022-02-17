/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_date.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/03 23:43:58 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/11 03:22:10 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_time.h"
#include "ft_printf.h"

char					*get_month(unsigned int month)
{
	static char*tab[] = {JANUARY, FEBRUARY, MARCH, APRIL, MAY, JUNE, JULY,
		AUGUST, SEPTEMBER, OCTOBER, NOVEMBER, DECEMBER};

	return (tab[month]);
}

unsigned int			get_days_month(unsigned int index)
{
	static unsigned int	tab[12] = {31, 28, 31, 30, 31,
		30, 31, 31, 30, 31, 30, 31};

	return (tab[index]);
}

char					*get_day(unsigned int n)
{
	static char			*tab[] = {THURSDAY, FRIDAY, SATURDAY, SUNDAY, MONDAY,
									TUESDAY, WEDNESDAY};

	return (tab[n % 7]);
}

void					get_dday(unsigned int n,
		unsigned int year_since, t_ftime *ptr)
{
	unsigned int		day_since_jan;
	unsigned int		month;
	unsigned int		t_day;

	month = 0;
	day_since_jan = ((n - (year_since / 4)) % 365) + 1;
	t_day = 0;
	while (month <= 11)
	{
		if (day_since_jan <= t_day)
			break ;
		t_day += get_days_month(month);
		month++;
	}
	ptr->month = month - 1;
	t_day -= get_days_month(month - 1);
	ptr->day = day_since_jan - t_day;
}

void					get_date(unsigned int n, t_ftime *ptr)
{
	unsigned int		year_since;
	unsigned int		day_since;

	if (n > 0)
		n += 19800;
	year_since = n / 31536000;
	ptr->sec = n % 60;
	n /= 60;
	ptr->min = n % 60;
	n /= 60;
	ptr->hour = n % 24;
	n /= 24;
	day_since = n;
	ptr->n = n;
	ptr->year = 1970 + (day_since / 365);
	get_dday(day_since, year_since, ptr);
}
