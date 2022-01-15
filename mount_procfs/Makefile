#
# Check mandatory vars
#

# defaults
FSNAME?=		procfs
FSVERSION?=		1.0
FSBUILD?=		1
BUNDLEDOMAIN?=	com.apple.filesystems.util
BUNDLEID?=		$(BUNDLEDOMAIN).$(FSNAME)
FSBUNDLE?=		$(FSNAME).fs
FSMACHO?=		$(FSNAME).out
ARCHFLAGS?=		-arch x86_64
PREFIX?=		/Library/Filesystems
DEVELOPER_DIR?=	/Applications/Xcode-13.1.app/Contents/Developer
SIGNCERT?=		sunnevanattsol@gmail.com

#
# Set default macOS SDK
# You may use
#  sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
# to switch to Xcode from Command Line Tools if cannot find any SDK
#
SDKROOT?=	$(shell xcrun --sdk macosx --show-sdk-path)

SDKFLAGS=	-isysroot $(SDKROOT)
CC=		$(shell xcrun -find -sdk $(SDKROOT) cc)
CODESIGN=	$(shell xcrun -find -sdk $(SDKROOT) codesign)

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
			-I$(DEVELOPER_DIR)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include

# warnings
CFLAGS+=	-Wall -Wextra

# linker flags
ifdef MACOSX_VERSION_MIN
LDFLAGS+=	-mmacosx-version-min=$(MACOSX_VERSION_MIN)
else
LDFLAGS+=	-mmacosx-version-min=11.3
endif
LDFLAGS+=	$(SDKFLAGS) \
			$(ARCHFLAGS)

# libraries
LIBS+=		-lutil

# fslibs flags
FSLFLAGS+=	-xml -c -unsupported -undef-symbols

# source, object files
SRCS:=		$(wildcard *.c)
OBJS:=		$(SRCS:.c=.o)

all: debug

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(FSMACHO): $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $^
	otool -h $@
	otool -l $@ | grep uuid

Info.plist~: Info.plist.in
	sed \
		-e 's/__FSNAME__/$(FSNAME)/g' \
		-e 's/__FSMACHO__/$(FSNAME)/g' \
		-e 's/__FSVERSION__/$(FSVERSION)/g' \
		-e 's/__FSBUILD__/$(FSBUILD)/g' \
		-e 's/__BUNDLEID__/$(BUNDLEID)/g' \
		-e 's/__OSBUILD__/$(shell /usr/bin/sw_vers -buildVersion)/g' \
		-e 's/__CLANGVER__/$(shell $(CC) -v 2>&1 | grep version)/g' \
	$^ > $@

$(FSBUNDLE): $(FSMACHO) Info.plist~
	mkdir -p $@/Contents/MacOS
	mv $< $@/Contents/MacOS/$(FSNAME)

	# Clear placeholders(o.w. fslibs cannot parse)
	sed 's/__FSLIBS__//g' Info.plist~ > $@/Contents/Info.plist
	awk '/__FSLIBS__/{system("fslibs $(FSLFLAGS) $@");next};1' Info.plist~ > $@/Contents/Info.plist~
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

	# Empty-dependency fs cannot be load  so we add one if necessary
	/usr/libexec/PlistBuddy -c 'Print OSBundleLibraries' $@/Contents/Info.plist &> /dev/null || \
		/usr/libexec/PlistBuddy -c 'Add :OSBundleLibraries:com.apple.kpi.bsd string "8.0b1"' $@/Contents/Info.plist

	touch $@

	dsymutil $(ARCHFLAGS) -o $(FSNAME).fs.dSYM $@/Contents/MacOS/$(FSNAME)

# see: https://www.gnu.org/software/make/manual/html_node/Target_002dspecific.html
# Those two flags must present at the same time  o.w. debug symbol cannot be generated
debug: CFLAGS += -O0
debug: $(FSBUNDLE)
	sudo chown -R root:wheel $<
	sudo chmod -R 755 $<

# see: https://stackoverflow.com/questions/15548023/clang-optimization-levels
release: CFLAGS += -O2
release: $(FSBUNDLE)
	sudo chown -R root:wheel $<
	sudo chmod -R 755 $<

install: $(FSBUNDLE) uninstall
	test -d "$(PREFIX)"
	sudo cp -r $< "$(PREFIX)/$<"
	sudo chown -R root:wheel "$(PREFIX)/$<"

uninstall:
	test -d "$(PREFIX)"
	test -e "$(PREFIX)/$(FSBUNDLE)" && \
	sudo rm -rf "$(PREFIX)/$(FSBUNDLE)" || true

clean:
	sudo rm -rf $(FSBUNDLE) $(FSBUNDLE).dSYM Info.plist~ $(OBJS) $(FSMACHO)

.PHONY: all debug release clean

