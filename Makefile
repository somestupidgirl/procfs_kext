#
# All-in-one Makefile
#
# Usage:
#   make                    # build for native arch (arm64e on Apple Silicon)
#   make ARCH=arm64e        # Apple Silicon kext + arm64 fs
#   make ARCH=x86_64        # Intel kext + x86_64 fs
#   make ARCH=universal     # fat binary (arm64e + x86_64)
#   make install            # install built bundles
#   make install ARCH=...   # build and install for specific arch
#

MAKE=make
OUT=bin

# Detect native arch if ARCH not specified
NATIVE_ARCH := $(shell uname -m)
ifeq ($(NATIVE_ARCH),arm64)
    DEFAULT_ARCH := arm64e
else
    DEFAULT_ARCH := x86_64
endif
ARCH ?= $(DEFAULT_ARCH)
# Accept arm64 as alias for arm64e (kexts require arm64e ABI)
ifeq ($(ARCH),arm64)
    override ARCH := arm64e
endif

# Per-arch settings
ifeq ($(ARCH),arm64e)
    KEXT_ARCHFLAGS    := -arch arm64e
    KEXT_TRIPLE       := arm64e-apple-macos12.0
    FS_ARCHFLAGS      := -arch arm64
    FS_TRIPLE         := arm64-apple-macos12.0
    LIB_ARCHFLAGS     := -arch arm64e
    LIB_TRIPLE        := arm64e-apple-macos12.0
else ifeq ($(ARCH),x86_64)
    KEXT_ARCHFLAGS    := -arch x86_64
    KEXT_TRIPLE       := x86_64-apple-macos10.15
    FS_ARCHFLAGS      := -arch x86_64
    FS_TRIPLE         := x86_64-apple-macos10.15
    LIB_ARCHFLAGS     := -arch x86_64
    LIB_TRIPLE        := x86_64-apple-macos10.15
else ifeq ($(ARCH),universal)
    $(error Universal kext builds require two-pass compilation + lipo. Not yet implemented. Build arm64e and x86_64 separately then lipo.)
else
    $(error Unknown ARCH=$(ARCH). Use arm64e, x86_64, or universal)
endif

KEXT_FLAGS := ARCHFLAGS="$(KEXT_ARCHFLAGS)" TARGET_TRIPLE="$(KEXT_TRIPLE)"
FS_FLAGS   := ARCHFLAGS="$(FS_ARCHFLAGS)"   TARGET_TRIPLE="$(FS_TRIPLE)"
LIB_FLAGS  := ARCHFLAGS="$(LIB_ARCHFLAGS)"  TARGET_TRIPLE="$(LIB_TRIPLE)"

all: debug

debug:
	rm -rf $(OUT)
	mkdir $(OUT)
	$(MAKE) -C lib $(LIB_FLAGS)
	$(MAKE) debug -C kext $(KEXT_FLAGS)
	mv kext/procfs.kext kext/procfs.kext.dSYM $(OUT)
	$(MAKE) debug -C fs $(FS_FLAGS)
	mv fs/procfs.fs fs/procfs.fs.dsym $(OUT)

release: TARGET=release
release: debug

install: debug
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
