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
	$(MAKE) -C mount_procfs $(TARGET)
	sudo mv mount_procfs/procfs.fs mount_procfs/procfs.fs.dsym $(OUT)


release: TARGET=release
release: debug

clean:
	sudo rm -rf $(OUT)
	sudo $(MAKE) -C kext clean
	sudo $(MAKE) -C mount_procfs clean

.PHONY: all debug release clean
