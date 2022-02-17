#
# Makefile for mount_emptyfs
#

DEVELOPER_DIR=/Applications/Xcode-13.1.app/Contents/Developer
SDKROOT=$(DEVELOPER_DIR)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
HEADERS=$(SDKROOT)/usr/include
INCLUDE=../include

CC=gcc
AR=ar

CFLAGS=-std=c99 -Wall -O3 -ffunction-sections -fdata-sections -Bstatic -std=c99 -I$(HEADERS) -I$(INCLUDE) -I.
SRCS = $(wildcard *.c)
LIB_OBJS = $(patsubst %.c,%.o,$(sort $(SRCS)))
LIB=libproc.a
RM=rm

all: $(LIB)

clean:
	@$(RM) $(LIB_OBJS) $(LIB)
	@echo "$(LIB_OBJS)" | xargs -n1 | ts "clean"

$(LIB):$(LIB_OBJS)
	@$(AR) rcs $@ $^
	@echo "$^" | xargs -n1 | ts "AR"
	@echo "AR $@"

%.o:%.c
	@$(CC) -o $@ $(CFLAGS) -c $< $(INC)
	@echo "$@" | xargs -n1 | ts "CC"
