# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: mmonier <marvin@42.fr>                     +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2018/11/09 10:47:53 by mmonier           #+#    #+#              #
#    Updated: 2019/11/03 22:18:12 by dbaffier         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

CC = gcc

CFLAGS = -Wall -Wextra -Werror

RM = rm -f

NAME = libft.a

HEADER = inc/

SRCS = 	ft_memset.c										\
	   	ft_bzero.c										\
	   	ft_memcpy.c										\
	   	ft_memccpy.c									\
	   	ft_memmove.c									\
	   	ft_memchr.c										\
	   	ft_memcmp.c 									\
	   	ft_strlen.c										\
	   	ft_strdup.c										\
	   	ft_strcpy.c										\
	   	ft_strncpy.c									\
	   	ft_strcat.c 									\
	   	ft_strncat.c 									\
	   	ft_strlcat.c 									\
	   	ft_strchr.c 									\
	   	ft_strrchr.c 									\
	   	ft_strstr.c 									\
	   	ft_strnstr.c 									\
	   	ft_strcmp.c 									\
		ft_strncmp.c 									\
	   	ft_atoi.c										\
		ft_isalpha.c 									\
		ft_isdigit.c 									\
		ft_isalnum.c 									\
		ft_isascii.c 									\
		ft_isprint.c 									\
		ft_toupper.c 									\
		ft_tolower.c 									\
		ft_memalloc.c 									\
		ft_memdel.c 									\
		ft_strnew.c 									\
		ft_strdel.c 									\
		ft_strclr.c 									\
		ft_striter.c 									\
		ft_striteri.c 									\
		ft_strmap.c 									\
		ft_strmapi.c 									\
		ft_strequ.c 									\
		ft_strnequ.c 									\
		ft_strsub.c 									\
		ft_strjoin.c 									\
		ft_strtrim.c 									\
		ft_strsplit.c 									\
		ft_itoa.c 										\
		ft_putchar.c 									\
		ft_putstr.c 									\
		ft_putendl.c 									\
		ft_putnbr.c 									\
		ft_putchar_fd.c 								\
		ft_putstr_fd.c 									\
		ft_putendl_fd.c 								\
		ft_putnbr_fd.c 									\
		ft_lstnew.c 									\
		ft_lstdelone.c 									\
		ft_lstdel.c 									\
		ft_lstadd.c 									\
		ft_lstiter.c 									\
		ft_lstmap.c 									\
		ft_reverse.c 									\
		ft_swap.c 										\
		ft_puttab.c 									\
		ft_isspace.c 									\
		ft_count_occ.c 									\
		ft_strnword.c 									\
		ft_putsnbr.c 									\
		ft_sort_all.c 									\
		ft_swap_all.c 									\
		ft_intcmp_void.c 								\
		ft_strcmp_void.c 								\
		ft_strjoinfree.c 								\
		ft_strjoin_c.c 									\
		ft_touppercase.c 								\
		ft_strrev.c										\
		ft_printf.c										\
		ft_dprintf.c									\
		ft_parse.c										\
		converter/get_converter.c						\
		converter/convert_c.c							\
		converter/convert_s.c							\
		converter/convert_o.c							\
		converter/convert_p.c							\
		converter/convert_x.c							\
		converter/convert_gx.c							\
		converter/convert_u.c							\
		converter/convert_f.c							\
		converter/convert_d.c							\
		converter/convert_b.c							\
		converter/convert_r.c							\
		converter/convert_gc.c							\
		converter/convert_gs.c							\
		converter/convert_gr.c							\
		converter/convert_k.c							\
		converter/convert_color.c						\
		converter/convert_star.c						\
		tools/float_to_string.c							\
		tools/ft_utoa.c									\
		tools/len_func.c								\
		tools/get_date.c								\
		tools/get_flags.c								\
		tools/write_wchar.c								\
		tools/get_valist.c								\
		tools/get_args.c								\
		tools/get_arg_int.c								\
		tools/convert_base.c							\
		tools/write_char.c								\
		tools/get_char_conv.c							\
		tools/apostrophe_flag.c							\
		tools/convert_ubase.c							\
		tools/ft_intmax_toa.c							\
		tools/check_mask.c								\
		tools/get_specifier.c							\
		get_next_line.c									\
		ft_strfjoin.c									\
		ft_rand.c										\
		ft_isnumber.c									\
		ft_round.c										\

OBJS = $(SRCS:.c=.o)

all: $(NAME)

%.o:%.c	
	@$(CC) -o $@ $(CFLAGS) -I $(HEADER) -c $<

$(NAME): $(OBJS)
	@ar rc $(NAME) $(OBJS)
	@ranlib $(NAME)

clean:
	@$(RM) $(OBJS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re

