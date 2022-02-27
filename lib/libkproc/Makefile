CC ?= clang
AR ?= ar -frs
RM ?= rm -f

CFLAGS?=	-std=c99 \
			-ffunction-sections \
			-fdata-sections \
			-Bstatic \
			-x c \
			-nostdinc \
			-fno-builtin \
			-fno-common \
			-mkernel \
			-Wall -Wextra -Wno-unused-function \
			-I../../include \
			-I../../lib \
			-I../../MacKernelSDK/Headers \
			-DDEBUG

EXT = a
LIB = libkproc.$(EXT)
SRCS = $(wildcard *.c)
LIB_OBJS = $(patsubst %.c,%.o,$(sort $(SRCS)))
INC  =  -I.

all: $(LIB)

clean:
	@$(RM) $(LIB_OBJS) $(LIB)
	@echo "$(LIB_OBJS)" | xargs -n1 | ts "clean"

$(LIB):$(LIB_OBJS)
	@$(AR) rcs $@ $^
	@echo "$^" | xargs -n1 | ts "AR"
	@echo "AR $@"

%.o:%.c
	@$(CC) -o $@ $(CFLAGS) -fPIC -fPIE -c $< $(INC)
	@echo "$@" | xargs -n1 | ts "CC"