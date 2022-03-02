#
# All-in-one Makefile
#

MAKE=make

all: debug

debug:
	$(MAKE) -C libutil

release: TARGET=release
release: debug

clean:
	$(MAKE) -C libutil clean

.PHONY: all debug release clean
