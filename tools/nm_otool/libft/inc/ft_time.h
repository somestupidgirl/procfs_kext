/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_time.h                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmonier <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/05/31 23:07:41 by mmonier           #+#    #+#             */
/*   Updated: 2019/06/11 05:59:49 by mmonier          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FT_TIME_H
# define FT_TIME_H

# define JANUARY "Jan"
# define FEBRUARY "Feb"
# define MARCH "Mar"
# define APRIL "Apr"
# define MAY "May"
# define JUNE "Jun"
# define JULY "Jul"
# define AUGUST "Aug"
# define SEPTEMBER "Sep"
# define OCTOBER "Oct"
# define NOVEMBER "Nov"
# define DECEMBER "Dec"

# define MONDAY "Mon"
# define TUESDAY "Tue"
# define WEDNESDAY "Wed"
# define THURSDAY "Thu"
# define FRIDAY "Fri"
# define SATURDAY "Sat"
# define SUNDAY "Sun"

typedef struct		s_ftime
{
	unsigned int	sec;
	unsigned int	min;
	unsigned int	hour;
	unsigned int	year;
	unsigned int	month;
	unsigned int	day;
	unsigned int	n;
}					t_ftime;

/*
** get_date.c
*/
char				*get_month(unsigned int month);
unsigned int		get_days_month(unsigned int index);
char				*get_day(unsigned int n);
void				get_dday(unsigned int n,
unsigned int		year_since, t_ftime *ptr);
void				get_date(unsigned int n, t_ftime *ptr);

#endif
