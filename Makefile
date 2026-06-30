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
    KEXT_ARCHFLAGS    := -arch arm64e
    KEXT_TRIPLE       := arm64e-apple-macos12.0
    FS_ARCHFLAGS      := -arch arm64
    FS_TRIPLE         := arm64-apple-macos12.0
    LIB_ARCHFLAGS     := -arch arm64e
    LIB_TRIPLE        := arm64e-apple-macos12.0
else
    $(error Unknown ARCH=$(ARCH). Use arm64e, x86_64, or universal)
endif

KEXT_FLAGS := ARCHFLAGS="$(KEXT_ARCHFLAGS)" TARGET_TRIPLE="$(KEXT_TRIPLE)"
FS_FLAGS   := ARCHFLAGS="$(FS_ARCHFLAGS)"   TARGET_TRIPLE="$(FS_TRIPLE)"
LIB_FLAGS  := ARCHFLAGS="$(LIB_ARCHFLAGS)"  TARGET_TRIPLE="$(LIB_TRIPLE)"

all: debug install-daemon stage-symbols tests

ifeq ($(ARCH),universal)

debug:
	rm -rf $(OUT)
	mkdir $(OUT)
	$(MAKE) -C lib  ARCHFLAGS="-arch arm64e" TARGET_TRIPLE="arm64e-apple-macos12.0"
	$(MAKE) debug -C kext ARCHFLAGS="-arch arm64e" TARGET_TRIPLE="arm64e-apple-macos12.0"
	$(MAKE) debug -C fs   ARCHFLAGS="-arch arm64"  TARGET_TRIPLE="arm64-apple-macos12.0"
	mv kext/procfs.kext kext/procfs.kext.dSYM fs/procfs.fs fs/procfs.fs.dsym $(OUT)
	mv $(OUT)/procfs.kext $(OUT)/procfs.kext.arm64e
	mv $(OUT)/procfs.fs   $(OUT)/procfs.fs.arm64
	$(MAKE) -C kext clean
	$(MAKE) -C fs clean
	$(MAKE) -C lib all clean
	$(MAKE) -C lib  ARCHFLAGS="-arch x86_64" TARGET_TRIPLE="x86_64-apple-macos10.15"
	$(MAKE) debug -C kext ARCHFLAGS="-arch x86_64" TARGET_TRIPLE="x86_64-apple-macos10.15"
	$(MAKE) debug -C fs   ARCHFLAGS="-arch x86_64" TARGET_TRIPLE="x86_64-apple-macos10.15"
	rm -rf $(OUT)/procfs.kext.dSYM $(OUT)/procfs.fs.dsym
	mv kext/procfs.kext kext/procfs.kext.dSYM fs/procfs.fs fs/procfs.fs.dsym $(OUT)
	mv $(OUT)/procfs.kext $(OUT)/procfs.kext.x86_64
	mv $(OUT)/procfs.fs   $(OUT)/procfs.fs.x86_64
	cp -r $(OUT)/procfs.kext.arm64e $(OUT)/procfs.kext
	lipo -create $(OUT)/procfs.kext.arm64e/Contents/MacOS/procfs $(OUT)/procfs.kext.x86_64/Contents/MacOS/procfs -output $(OUT)/procfs.kext/Contents/MacOS/procfs
	cp -r $(OUT)/procfs.fs.arm64 $(OUT)/procfs.fs
	lipo -create $(OUT)/procfs.fs.arm64/Contents/Resources/mount_procfs $(OUT)/procfs.fs.x86_64/Contents/Resources/mount_procfs -output $(OUT)/procfs.fs/Contents/Resources/mount_procfs
	codesign --force --timestamp=none --sign - $(OUT)/procfs.kext
	codesign --force --timestamp=none --sign - $(OUT)/procfs.fs
	rm -rf $(OUT)/procfs.kext.arm64e $(OUT)/procfs.kext.x86_64
	rm -rf $(OUT)/procfs.fs.arm64 $(OUT)/procfs.fs.x86_64

else

debug:
	rm -rf $(OUT)
	mkdir $(OUT)
	$(MAKE) -C lib $(LIB_FLAGS)
	$(MAKE) debug -C kext $(KEXT_FLAGS)
	mv kext/procfs.kext kext/procfs.kext.dSYM $(OUT)
	$(MAKE) debug -C fs $(FS_FLAGS)
	mv fs/procfs.fs fs/procfs.fs.dsym $(OUT)

endif

release: TARGET=release
release: debug

install: debug install-only install-daemon stage-symbols

install-only: stage-symbols install-daemon
	cp -r $(OUT)/procfs.kext /Library/Extensions
	cp -r $(OUT)/procfs.fs /Library/Filesystems
	chmod -R 755 /Library/Extensions/procfs.kext
	chown -R root:wheel /Library/Extensions/procfs.kext
	chmod -R 755 /Library/Filesystems/procfs.fs
	chown -R root:wheel /Library/Filesystems/procfs.fs

# Build + install the userspace daemon (procfsd: serves proc_pidinfo to the kext
# via the kernel-control bridge, stages symbols, optionally loads the kext, and
# keeps the console user's ~/proc mounted - all as root) and its LaunchDaemon.
# Auto-load and auto-mount stay DISARMED until the operator creates
# /var/db/procfs.enabled - so a kext panic during development cannot boot-loop
# the machine. The plist is RunAtLoad, so procfsd starts on the next boot.
install-daemon:
	@mkdir -p $(OUT)
	cc -O2 -Wall -o $(OUT)/procfsd tools/procfsd.c
	cc -O2 -Wall -o $(OUT)/procfs_ksyms tools/procfs_ksyms.c -lcompression
	install -d -m 755 /usr/local/sbin
	install -m 755 $(OUT)/procfsd        /usr/local/sbin/procfsd
	install -m 755 $(OUT)/procfs_ksyms   /usr/local/sbin/procfs_ksyms
	install -m 644 -o root -g wheel tools/com.beako.procfsd.plist /Library/LaunchDaemons/com.beako.procfsd.plist
	@# Create the /proc mount point on the read-only system volume via synthetic.conf.
	@grep -qxF 'proc' /etc/synthetic.conf 2>/dev/null || printf 'proc\n' >> /etc/synthetic.conf
	@echo "procfs: ensured 'proc' in /etc/synthetic.conf -> /proc is created at boot."
	@echo "procfs: daemon + LaunchDaemon installed (auto-load/mount DISARMED)."
	@echo "procfs: to arm auto-load + mount:  sudo touch /var/db/procfs.enabled"
	@echo "procfs: then REBOOT (creates /proc + loads kext + procfsd mounts /proc)."

# Build and run the userspace symbol-staging helper. It decompresses the booted
# kernelcache and writes /var/db/procfs.ksyms, from which the kext resolves the
# private symbols it cannot link or scan for directly. Re-run after a kernel
# update; the kext safely ignores a stale file. Non-fatal if it fails (the
# klookup-gated features just stay disabled).
stage-symbols:
	@echo "Staging kernel symbols (libklookup)..."
	@mkdir -p $(OUT)
	cc -O2 -Wall -o $(OUT)/procfs_ksyms tools/procfs_ksyms.c -lcompression
	$(OUT)/procfs_ksyms || echo "procfs: symbol staging failed; klookup features disabled"

tests:
	$(MAKE) -C test

clean:
	rm -rf $(OUT)
	$(MAKE) -C kext clean
	$(MAKE) -C fs clean
	$(MAKE) -C lib clean
	$(MAKE) -C test clean

clean-tests:
	$(MAKE) -C test clean

.PHONY: all debug release install install-only install-daemon stage-symbols clean tests clean-tests
