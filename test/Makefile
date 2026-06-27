PROC_PATH?= /Users/sunneva/proc

CC=     cc
CFLAGS= -Wall -Wextra -std=c99 -g -DPROC_PATH=\"$(PROC_PATH)\"

PROGS=  test_mount test_readdir test_getdents

all: $(PROGS)

test_mount: test_mount.c
	$(CC) $(CFLAGS) -o $@ $<

test_readdir: test_readdir.c
	$(CC) $(CFLAGS) -o $@ $<

test_getdents: test_getdents.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)
	rm -rf *.dSYM

.PHONY: all clean
