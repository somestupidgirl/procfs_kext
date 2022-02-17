# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: dbaffier <marvin@42.fr>                    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2019/10/24 00:18:28 by dbaffier          #+#    #+#              #
#    Updated: 2019/12/12 23:56:22 by dbaffier         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NM = ft_nm
OTOOL = ft_otool

CFLAGS += -Wall -Werror -Wextra

LIBFT_PATH = libft
LIBFT_LIB = $(LIBFT_PATH)/libft.a
LIBFT_LINK = -L$(LIBFT_PATH) -lft

INC_DIR = inc/
INCS = -I$(LIBFT_PATH)/inc -I$(INC_DIR)

OBJS_NM_DIR = objs_nm/
OBJS_NM = $(addprefix $(OBJS_NM_DIR), $(SRCS_NM:.c=.o))

OBJS_OT_DIR = objs_otool/
OBJS_OT = $(addprefix $(OBJS_OT_DIR), $(SRCS_OT:.c=.o))

OBJS_STUFF_DIR = objs_stuff/
OBJS_STUFF = $(addprefix $(OBJS_STUFF_DIR), $(SRCS_STUFF:.c=.o))

STUFF_DIR = stuff/
SRCS_STUFF = byte_sex.c				\
			 member_tools.c			\
		  	 ft_archive.c			\
		  	 ft_mach_o.c			\
		  	 ft_fat.c				\
			 ofile_member.c			\
			 ofile_first_member.c	\
			 ofile_member_clear.c	\
			 ofile_process.c		\
			 ofile_unmap.c			\
			 swap_bytes.c			\
			 swap_bytes_h.c			\
			 swap_header.c			\
			 print_fat.c			\
			 macho.c				\

SRCS_NM_DIR = nm/
SRCS_NM = main.c					\
		  err_file.c				\
		  nm.c						\
		  symbol.c					\
		  print.c					\
		  symbol_type.c				\
		  process_flag.c			\
		  parse_flag.c				\
		  stab.c					\

SRCS_OT_DIR = otool/
SRCS_OT = main.c					\
		  processor.c				\
		  parse_flag.c				\
		  print_header.c			\
		  print_loadcmds.c			\
		  print_lc_seg32.c			\
		  print_lc_seg64.c			\
		  print_lc_symtab.c			\
		  print_libraries.c			\
		  get_sect_info.c			\
		  print_text.c				\

all: $(OBJS_NM_DIR) $(OBJS_STUFF_DIR) $(OBJS_OT_DIR) $(LIBFT_LIB) $(NM) $(OTOOL)

nm: $(OBJS_NM_DIR) $(OBJS_STUFF_DIR)  $(LIBFT_LIB) $(NM)

otool: $(OBJS_STUFF_DIR) $(OBJS_OT_DIR) $(LIBFT_LIB) $(OTOOL)

$(OBJS_STUFF_DIR):
	@mkdir -p $@

$(OBJS_NM_DIR):
	@mkdir -p $@

$(OBJS_OT_DIR):
	@mkdir -p $@

$(LIBFT_LIB):
	@make -C $(LIBFT_PATH)

$(OTOOL): $(OBJS_STUFF) $(OBJS_OT)
	gcc $^ -o $@ $(LIBFT_LINK)

$(NM): $(OBJS_NM) $(OBJS_STUFF)
	gcc $^ -o $@ $(LIBFT_LINK)

$(OBJS_OT_DIR)%.o: $(SRCS_OT_DIR)%.c
	gcc $(CFLAGS) -o $@ -c $< $(INCS)

$(OBJS_STUFF_DIR)%.o: $(STUFF_DIR)%.c
	gcc $(CFLAGS) -o $@ -c $< $(INCS)

$(OBJS_NM_DIR)%.o: $(SRCS_NM_DIR)%.c
	gcc $(CFLAGS) -o $@ -c $< $(INCS)

clean:
	@make -C $(LIBFT_PATH) clean
	rm -rf $(OBJS_STUFF_DIR)
	rm -rf $(OBJS_NM_DIR)
	rm -rf $(OBJS_OT_DIR)

fclean: clean
	rm -f $(LIBFT_LIB)
	rm -f $(NM)
	rm -f $(OTOOL)

re: fclean all


.PHONY: fclean all otool nm clean re
