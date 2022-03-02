#
# All-in-one Makefile
#

MAKE=make
OUT=bin

all: debug

debug:
	rm -rf $(OUT)
	mkdir $(OUT)
	$(MAKE) -C lib
	$(MAKE) debug -C kext $(TARGET)
	mv kext/procfs.kext kext/procfs.kext.dSYM $(OUT)
	$(MAKE) debug -C fs $(TARGET)
	mv fs/procfs.fs fs/procfs.fs.dsym $(OUT)


release: TARGET=release
release: debug

install:
	sudo cp -r $(OUT)/procfs.kext /Library/Extensions
	sudo cp -r $(OUT)/procfs.fs /Library/Filesystems
	sudo chmod -R 755 /Library/Extensions/procfs.kext
	sudo chown -R root:wheel /Library/Extensions/procfs.kext
	sudo chmod -R 755 /Library/Filesystems/procfs.fs
	sudo chown -R root:wheel /Library/Filesystems/procfs.fs

clean:
	rm -rf $(OUT)
	$(MAKE) -C kext clean
	$(MAKE) -C fs clean
	$(MAKE) -C lib clean

.PHONY: all debug release install clean
