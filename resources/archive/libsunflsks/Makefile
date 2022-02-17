TARGET := iphone:clang:latest:13.3

include $(THEOS)/makefiles/common.mk
ARCHS = arm64 arm64e
LIBRARY_NAME = libsunflsks
libsunflsks_FILES = $(wildcard src/*.m lib/*.m)
libsunflsks_CFLAGS = -fobjc-arc -Wno-deprecated -Wall -Werror
libsunflsks_INSTALL_PATH = /usr/lib
libsunflsks_FRAMEWORKS = SystemConfiguration

include $(THEOS_MAKE_PATH)/library.mk

ifeq ($(DEBUG),1)
libsunflsks_SUBPROJECTS += sysstat
endif
