#
# All-in-one Makefile
#

MAKE=make
OUT=bin

all: debug

debug:
	sudo rm -rf $(OUT)
	mkdir $(OUT)
	$(MAKE) -C kext $(TARGET)
	sudo mv kext/procfs.kext kext/procfs.kext.dSYM $(OUT)
	$(MAKE) -C fs $(TARGET)
	sudo mv fs/procfs.fs fs/procfs.fs.dsym $(OUT)


release: TARGET=release
release: debug

clean:
	sudo rm -rf $(OUT)
	sudo $(MAKE) -C kext clean
	sudo $(MAKE) -C fs clean

.PHONY: all debug release clean
