#
# All-in-one Makefile
#

MAKE=make
OUT=bin

all: debug

debug:
	sudo rm -rf $(OUT)
	mkdir $(OUT)
	$(MAKE) debug -C kext $(TARGET)
	sudo mv kext/procfs.kext kext/procfs.kext.dSYM $(OUT)
	$(MAKE) debug -C fs $(TARGET)
	sudo mv fs/procfs.fs fs/procfs.fs.dsym $(OUT)


release: TARGET=release
release: debug

install:
	sudo cp -r $(OUT)/procfs.kext /Library/Extensions
	sudo cp -r $(OUT)/procfs.fs /Library/Filesystems

clean:
	sudo rm -rf $(OUT)
	sudo $(MAKE) -C kext clean
	sudo $(MAKE) -C fs clean

.PHONY: all debug release install clean
