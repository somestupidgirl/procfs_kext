TARGET := iphone:clang:latest:7.0

include $(THEOS)/makefiles/common.mk

TOOL_NAME = sysstat
ARCHS = arm64 arm64e

sysstat_FILES = main.m
sysstat_CFLAGS = -fobjc-arc -Wno-unused-variable -I../include
sysstat_CODESIGN_FLAGS = -Sent.xml
sysstat_INSTALL_PATH = /usr/bin
sysstat_FRAMEWORKS = UIKit
sysstat_LIBRARIES = sunflsks
sysstat_LIBRARY_PATH = $(shell pwd)/../.theos/obj/debug
include $(THEOS_MAKE_PATH)/tool.mk
