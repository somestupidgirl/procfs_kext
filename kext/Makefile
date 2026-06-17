include ../Makefile.inc

SDKROOT?=       $(shell xcrun --sdk macosx --show-sdk-path)
KERNEL_FW=      $(SDKROOT)/System/Library/Frameworks/Kernel.framework
KERNEL_HEADERS= $(KERNEL_FW)/Headers
KERNEL_LIBDIR=  $(SDKROOT)/usr/lib

BUNDLEID?=           $(BUNDLEDOMAIN).$(KEXTNAME)
KEXTBUNDLE?=         $(KEXTNAME).kext
KEXTMACHO?=          $(KEXTNAME).out

ARCHFLAGS?=          -arch arm64e
TARGET_TRIPLE?=      arm64e-apple-macos12.0

PREFIX?=             /Library/Extensions
DEVELOPER_DIR?=      /Applications/Xcode.app/Contents/Developer
MACOSX_VERSION_MIN?= 12.0

CC=       $(shell xcrun -find -sdk $(SDKROOT) cc)
CODESIGN= $(shell xcrun -find -sdk $(SDKROOT) codesign)

SRCS:= $(wildcard *.c) $(wildcard **/*.c)
OBJS:= $(SRCS:.c=.o)

CFLAGS+= -DKEXTNAME_S=\"$(KEXTNAME)\" \
         -DKEXTVERSION_S=\"$(KEXTVERSION)\" \
         -DKEXTBUILD_S=\"$(KEXTBUILD)\" \
         -DBUNDLEID_S=\"$(BUNDLEID)\" \
         -DBUNDLEID=$(BUNDLEID)

TIME_STAMP:= $(shell date +'%Y/%m/%d\ %H:%M:%S%z')
CFLAGS+=     -D__TS__=\"$(TIME_STAMP)\"

CFLAGS+= -target $(TARGET_TRIPLE) \
         -isysroot $(SDKROOT) \
         $(ARCHFLAGS) \
         -x c \
         -std=c99 \
         -nostdinc \
         -fno-builtin \
         -fno-common \
         -mkernel \
         -fno-stack-check \
         -fno-stack-protector \
         -I../kext/lib \
         -I../lib \
         -I../include \
         -isystem $(KERNEL_HEADERS) \
         -isystem $(KERNEL_HEADERS)/arm64 \
         -isystem $(KERNEL_HEADERS)/arm

CFLAGS+= -Werror -Wall -Wextra \
         -Wno-unused-parameter \
         -Wno-unused-variable \
         -Wno-unused-value \
         -Wno-vla

LDFLAGS+= -target $(TARGET_TRIPLE) \
          -isysroot $(SDKROOT) \
          $(ARCHFLAGS) \
          -nostdlib \
          -Xlinker -kext \
          -Xlinker -object_path_lto lib/cpu.o \
          -Xlinker -object_path_lto lib/kern.o \
          -Xlinker -object_path_lto lib/symbols.o \
          -Xlinker -object_path_lto procfs.o \
          -Xlinker -object_path_lto procfs_cmdline.o \
          -Xlinker -object_path_lto procfs_data.o \
          -Xlinker -object_path_lto procfs_linux.o \
          -Xlinker -object_path_lto procfs_node.o \
          -Xlinker -object_path_lto procfs_structure.o \
          -Xlinker -object_path_lto procfs_subr.o \
          -Xlinker -object_path_lto procfs_vfsops.o \
          -Xlinker -object_path_lto procfs_vnops.o \
          -Xlinker -export_dynamic

LIBS+= -L$(KERNEL_LIBDIR) \
       -L../lib/libkext \
       -L../lib/libklookup

LIBS+= -lkmod -lkext -lklookup

KLFLAGS+= -xml -c -undef-symbols

all: debug

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(KEXTMACHO): $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $^
	otool -h $@
	otool -l $@ | grep uuid

Info.plist~: Info.plist.in
	sed \
		-e 's/__KEXTNAME__/$(KEXTNAME)/g' \
		-e 's/__KEXTMACHO__/$(KEXTNAME)/g' \
		-e 's/__KEXTVERSION__/$(KEXTVERSION)/g' \
		-e 's/__KEXTBUILD__/$(KEXTBUILD)/g' \
		-e 's/__BUNDLEID__/$(BUNDLEID)/g' \
		-e 's/__OSBUILD__/$(shell /usr/bin/sw_vers -buildVersion)/g' \
		-e 's/__CLANGVER__/$(shell $(CC) -v 2>&1 | grep version)/g' \
	$^ > $@

$(KEXTBUNDLE): $(KEXTMACHO) Info.plist~
	mkdir -p $@/Contents/MacOS
	mv $< $@/Contents/MacOS/$(KEXTNAME)
	cp Info.plist~ $@/Contents/Info.plist
ifdef COMPATIBLE_VERSION
	/usr/libexec/PlistBuddy -c 'Add :OSBundleCompatibleVersion string "$(COMPATIBLE_VERSION)"' $@/Contents/Info.plist
endif
ifdef COPYRIGHT
	/usr/libexec/PlistBuddy -c 'Add :NSHumanReadableCopyright string "$(COPYRIGHT)"' $@/Contents/Info.plist
endif
ifdef SIGNCERT
	$(CODESIGN) --force --timestamp=none --sign $(SIGNCERT) $@
	/usr/libexec/PlistBuddy -c 'Add :CFBundleSignature string ????' $@/Contents/Info.plist
endif
ifdef MACOSX_VERSION_MIN
	/usr/libexec/PlistBuddy -c 'Add :LSMinimumSystemVersion string "$(MACOSX_VERSION_MIN)"' $@/Contents/Info.plist
endif
	/usr/libexec/PlistBuddy -c 'Print OSBundleLibraries' $@/Contents/Info.plist &> /dev/null || \
		/usr/libexec/PlistBuddy -c 'Add :OSBundleLibraries:com.apple.kpi.bsd string "8.0b1"' $@/Contents/Info.plist
	touch $@
	dsymutil $(ARCHFLAGS) -o $(KEXTNAME).kext.dSYM $@/Contents/MacOS/$(KEXTNAME)

debug: CFLAGS += -O0 -g
debug: $(KEXTBUNDLE)

release: CFLAGS += -O2
release: $(KEXTBUNDLE)

load: $(KEXTBUNDLE)
	sudo chown -R root:wheel $<
	sudo sync
	sudo kextutil $<
	sudo chown -R '$(USER):$(shell id -gn)' $<
	sudo dmesg | grep $(KEXTNAME) | tail -1

stat:
	kextstat | grep $(KEXTNAME)

unload:
	sudo kextunload $(KEXTBUNDLE)
	sudo dmesg | grep $(KEXTNAME) | tail -2

install: $(KEXTBUNDLE) uninstall
	test -d "$(PREFIX)"
	sudo cp -r $< "$(PREFIX)/$<"
	sudo chmod -R 755 "$(PREFIX)/$<"
	sudo chown -R root:wheel "$(PREFIX)/$<"

uninstall:
	test -d "$(PREFIX)"
	test -e "$(PREFIX)/$(KEXTBUNDLE)" && \
	sudo rm -rf "$(PREFIX)/$(KEXTBUNDLE)" || true

clean:
	rm -rf $(KEXTBUNDLE) $(KEXTBUNDLE).dSYM Info.plist~ $(OBJS) $(KEXTMACHO)

.PHONY: all debug release load stat unload install uninstall clean
