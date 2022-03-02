#
# Check mandatory vars
#

# defaults
COPYRIGHT?=				"Copyright © 2022 somestupidgirl,  All rights reserved."
KEXTNAME?=				procfs
KEXTVERSION?=			1.0.0
KEXTBUILD?=				1
BUNDLEDOMAIN?=			com.stupid.filesystems
BUNDLEID?=				$(BUNDLEDOMAIN).$(KEXTNAME)
KEXTBUNDLE?=			$(KEXTNAME).kext
KEXTMACHO?=				$(KEXTNAME).out
ARCHFLAGS?=				-arch x86_64
PREFIX?=				/Library/Extensions
DEVELOPER_DIR?=			/Applications/Xcode.app/Contents/Developer
SIGNCERT?=				foozlefairy@gmail.com
MACOSX_VERSION_MIN?= 	11.3

#
# Set default macOS SDK
# You may use
#  sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
# to switch to Xcode from Command Line Tools if cannot find any SDK
#
SDKROOT?=	$(shell xcrun --sdk macosx --show-sdk-path)

SDKFLAGS=	-isysroot $(SDKROOT)
CC=		$(shell xcrun -find -sdk $(SDKROOT) cc)
#CXX=		$(shell xcrun -find -sdk $(SDKROOT) c++)
CODESIGN=	$(shell xcrun -find -sdk $(SDKROOT) codesign)

# source, object files
SRCS:=		$(wildcard *.c)
OBJS:=		$(SRCS:.c=.o)

#
# Convenience defines
# BUNDLEID macro will be used in KMOD_EXPLICIT_DECL
#
CFLAGS+=	-DKEXTNAME_S=\"$(KEXTNAME)\"		\
			-DKEXTVERSION_S=\"$(KEXTVERSION)\"	\
			-DKEXTBUILD_S=\"$(KEXTBUILD)\"		\
			-DBUNDLEID_S=\"$(BUNDLEID)\"		\
			-DBUNDLEID=$(BUNDLEID)

TIME_STAMP:=	$(shell date +'%Y/%m/%d\ %H:%M:%S%z')
CFLAGS+=		-D__TS__=\"$(TIME_STAMP)\"

#
# C compiler flags
#
ifdef MACOSX_VERSION_MIN
CFLAGS+=	-mmacosx-version-min=$(MACOSX_VERSION_MIN)
else
CFLAGS+=	-mmacosx-version-min=11.3
endif
CFLAGS+=	$(SDKFLAGS) \
			$(ARCHFLAGS) \
			-x c \
			-std=c99 \
			-nostdinc \
			-fno-builtin \
			-fno-common \
			-mkernel \
			-I../include \
			-I../lib \
			-I../MacKernelSDK/Headers

# warnings
CFLAGS+=	-Wall -Wextra -Wno-vla -Wno-nullability-completeness

# linker flags
ifdef MACOSX_VERSION_MIN
LDFLAGS+=	-mmacosx-version-min=$(MACOSX_VERSION_MIN)
else
LDFLAGS+=	-mmacosx-version-min=11.3
endif
LDFLAGS+= 	$(SDKFLAGS) \
			$(ARCHFLAGS) \
			-nostdlib \
			-Xlinker -kext \
			-Xlinker -object_path_lto main.o \
			-Xlinker -object_path_lto procfs_data.o \
			-Xlinker -object_path_lto procfs_info.o \
			-Xlinker -object_path_lto procfs_node.o \
			-Xlinker -object_path_lto procfs_structure.o \
			-Xlinker -object_path_lto procfs_subr.o \
			-Xlinker -object_path_lto procfs_vfsops.o \
			-Xlinker -object_path_lto procfs_vnops.o \
			-Xlinker -export_dynamic

# libraries
LIBS+=		-L../MacKernelSDK/Library/x86_64 \
			-L../lib/libkext \
			-L../lib/libklookup

LIBS+=		-lcc_kext -lkmod -lkext -lklookup

# kextlibs flags
KLFLAGS+=	-xml -c -undef-symbols -unsupported

# targets

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

	# Clear placeholders(o.w. kextlibs cannot parse)
	sed 's/__KEXTLIBS__//g' Info.plist~ > $@/Contents/Info.plist
	awk '/__KEXTLIBS__/{system("kextlibs $(KLFLAGS) $@");next};1' Info.plist~ > $@/Contents/Info.plist~
	mv $@/Contents/Info.plist~ $@/Contents/Info.plist

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

	# Empty-dependency kext cannot be load  so we add one if necessary
	/usr/libexec/PlistBuddy -c 'Print OSBundleLibraries' $@/Contents/Info.plist &> /dev/null || \
		/usr/libexec/PlistBuddy -c 'Add :OSBundleLibraries:com.apple.kpi.bsd string "8.0b1"' $@/Contents/Info.plist

	touch $@

	dsymutil $(ARCHFLAGS) -o $(KEXTNAME).kext.dSYM $@/Contents/MacOS/$(KEXTNAME)

# see: https://www.gnu.org/software/make/manual/html_node/Target_002dspecific.html
# Those two flags must present at the same time  o.w. debug symbol cannot be generated
debug: CFLAGS += -O0 -g
debug: $(KEXTBUNDLE)

# see: https://stackoverflow.com/questions/15548023/clang-optimization-levels
release: CFLAGS += -O2
release: $(KEXTBUNDLE)

load: $(KEXTBUNDLE)
	sudo chown -R root:wheel $<
	sudo sync
	sudo kextutil $<
	# restore original owner:group
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
	sudo chmod -R  755 "$(PREFIX)/$<"
	sudo chown -R root:wheel "$(PREFIX)/$<"

uninstall:
	test -d "$(PREFIX)"
	test -e "$(PREFIX)/$(KEXTBUNDLE)" && \
	sudo rm -rf "$(PREFIX)/$(KEXTBUNDLE)" || true

clean:
	rm -rf $(KEXTBUNDLE) $(KEXTBUNDLE).dSYM Info.plist~ $(OBJS) $(KEXTMACHO)

.PHONY: all debug release load stat unload intall uninstall clean

