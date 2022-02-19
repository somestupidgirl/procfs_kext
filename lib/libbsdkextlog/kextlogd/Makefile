#
# Makefile  Created 190417
#

CC?=gcc
CFLAGS+=-std=c99 -xc -Wall -Wextra -Werror -I../../lib

all: debug

kextlog_daemon.o: kextlogd.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -c

kextlog_daemon: kextlogd.o
	$(CC) -o $@ $^

release: CFLAGS += -O2
release: kextlogd

kextlog_daemon-debug: kextlogd.o
	$(CC) -o $@ $^

debug: CPPFLAGS += -DDEBUG
debug: CFLAGS += -g -O0
debug: kextlogd

clean:
	rm -f *.o kextlog_daemon kextlog_daemon-debug

