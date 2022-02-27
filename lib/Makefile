#
# All-in-one Makefile
#

MAKE=make

all: debug

debug:
	$(MAKE) -C libkext
	$(MAKE) -C libklookup
	$(MAKE) -C libproc
	$(MAKE) -C libutil


release: TARGET=release
release: debug

clean:
	$(MAKE) -C libkext clean
	$(MAKE) -C libklookup clean
	$(MAKE) -C libproc clean
	$(MAKE) -C libutil clean

.PHONY: all debug release clean
