#
# All-in-one Makefile
#

MAKE=make

all: debug

debug:
	$(MAKE) -C libkext
	$(MAKE) -C libklookup

release: TARGET=release
release: debug

clean:
	$(MAKE) -C libkext clean
	$(MAKE) -C libklookup clean

.PHONY: all debug release clean
