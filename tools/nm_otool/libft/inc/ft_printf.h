/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_printf.h                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/06/10 04:33:29 by dbaffier          #+#    #+#             */
/*   Updated: 2019/11/10 21:14:05 by dbaffier         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FT_PRINTF_H
# define FT_PRINTF_H

# include <stdlib.h>
# include <stdarg.h>
# include "libft.h"
# include "converter.h"

# define BUFF_SIZE 1024

# define OPT_MINUS 1
# define OPT_PLUS 2
# define OPT_ZERO 4
# define OPT_SPACE 8
# define OPT_HASH 16
# define OPT_PREC 32
# define OPT_STAR 64
# define OPT_HSTAR 128
# define OPT_APS 256

# define OPT_H 1
# define OPT_HH 2
# define OPT_L 4
# define OPT_LL 8
# define OPT_J 16
# define OPT_Z 32

typedef struct			s_flag
{
	char				star;
	short				id;
	int					opts;
	int					size_modifier;
	size_t				precision;
	size_t				width;
}						t_flag;

typedef struct			s_va_list
{
	int					type;
	void				(*func)(void *ptr, va_list *args);
	int					id;
	struct s_va_list	*next;
}						t_va_list;

typedef struct			s_format
{
	unsigned int	pos;
	unsigned int	tmp_pos;
	int				len;
	int				ret;
	int				*fd;
	short			id;
	va_list			arg;
	va_list			copy;
	char			*tail;
	char			*head;
	void			(*print_buff)(struct s_format *data);
	char			buff[BUFF_SIZE];
	t_flag			flag;
	t_va_list		*lst;
}						t_format;

typedef struct			s_color
{
	char				*color;
	char				*code;
}						t_color;

typedef struct			s_convert
{
	char				type;
	void				(*func)(t_format *data);
}						t_convert;

typedef struct			s_getarg
{
	int					type;
	void				(*func)(void *ptr, va_list *args);
}						t_getarg;

/*
** src/converter/ -- convert_{c, s, p, d, i, o, u, x, X, f}.c
*/

void					convert_c(t_format *data);
void					convert_s(t_format *data);
void					convert_p(t_format *data);
void					convert_d(t_format *data);
void					convert_i(t_format *data);
void					convert_o(t_format *data);
void					convert_u(t_format *data);
void					convert_x(t_format *data);
void					convert_f(t_format *data);
void					convert_gx(t_format *data);
void					convert_gc(t_format *data);
void					convert_gs(t_format *data);
void					convert_z(t_format *data);
void					convert_b(t_format *data);
void					convert_r(t_format *data);
void					convert_gr(t_format *data);
void					convert_k(t_format *data);
void					convert_m(t_format *data);
void					conv_star(t_format *data);

/*
** src/ -- buff_func.c
*/
void					ft_parse(t_format *data);
void					getarg_null(void *dst, va_list *args);
void					getarg_float(void *ptr, va_list *args);
void					getarg_ptr(void *ptr, va_list *args);
void					getarg_int(void *ptr, va_list *args);
void					getarg_wchar(void *ptr, va_list *args);
void					getarg_wstr(void *ptr, va_list *args);
void					getarg_char(void *ptr, va_list *args);
void					getarg_uint(void *ptr, va_list *args);
void					getarg_long(void *dst, va_list *args);
void					getarg_uintmaxt(void *ptr, va_list *args);

/*
** src/converter/ --  convert_color.c
*/
int						get_color(t_format *data);
void					create_node(t_va_list **head, t_format *data, int type);

/*
** src/tools/ --
*/
int						get_converter(t_format *data);
void					get_flags(t_format *data);
void					get_varg(t_format *data);
void					get_arg_n(t_format *data, void *dst);
char					get_char_conv(t_format *data);

/*
** src/tools/ -- ft_utoa.c
*/
char					*ft_utoa(int n);
char					*ft_intmax_toa(intmax_t n);
void					ft_parse(t_format *data);
void					ft_preparse(t_format *data);

/*
** src/ -- ft_printf.c
*/
int						ft_printf(const char *str, ...);
int						ft_dprintf(int fd, const char *str, ...);

/*
** src/tools -- write_wchar.c
*/
size_t					ft_wstrlen(const wchar_t *s);
void					write_wchar(t_format *data, wchar_t c);
void					ft_putwchar(t_format *data, wchar_t c);
void					ft_putwstr(t_format *data, wchar_t *str, size_t n);
void					get_valist(t_format *data);

/*
** src/tools -- write_char.c
*/
void					write_str(t_format *data, char *str);
void					write_nstr(t_format *data, char *str, size_t len);
void					write_char(t_format *data, char c);
void					write_float(t_format *data, char c, int j);
void					write_nbr(t_format *data, uintmax_t base,
		uintmax_t n, char letter);

/*
** src/tools/ -- convert_base.c
*/
char					*ft_convert_base(intmax_t n,
		int base, size_t n_size, char letter);

/*
** src/tools/ -- convert_ubase.c
*/
char					*ft_convert_ubase(uintmax_t n,
		int base, size_t n_size, char letter);

/*
** src/tools/ -- float_to_string.c
*/
char					*float_to_string(t_format *data, t_convf *ptr);

/*
** src/tools/ -- len_func.c
*/
void					ft_doublelen(t_convf *ptr);
size_t					ft_intlen_base(intmax_t n, int base);
size_t					ft_uintlen_base(uintmax_t n, int base);

/*
** src/tools/ -- apostrophe_flag.c
*/
void					get_aps(t_format *data, char *str, unsigned int len);

/*
** src/tools/ -- check_mask.c
*/
int						msk(t_format *data, int mask);

/*
** src/tools/ -- get_specifier.c
*/
int						get_specifier(t_format *data);

#endif
