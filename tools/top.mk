ifneq ($(lastword a b),b)
$(error This Makefile requires make 3.81 or newer)
endif

# Detect whether shell style is windows or not
# https://stackoverflow.com/questions/714100/os-detecting-makefile/52062069#52062069
ifeq '$(findstring ;,$(PATH))' ';'
# PATH contains semicolon - so we're definitely on Windows.
CMDEXE := 1

# makefile shell commands should use syntax for DOS CMD, not unix sh
# Unfortunately, SHELL may point to sh or bash, which can't accept DOS syntax.
# We can't just use sh, because while sh and/or bash shell may be available,
# many Windows environments won't have utilities like realpath used below, so...
# Force DOS command shell on Windows.
SHELL := cmd.exe
endif

# Set TOP to be the path to get from the current directory (where make was
# invoked) to the top of the tree. $(lastword $(MAKEFILE_LIST)) returns
# the name of this makefile relative to where make was invoked.
THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# strip off /tools/top.mk to get for example ../../..
# and Set TOP to an absolute path
TOP = $(abspath $(subst /tools/top.mk,,$(THIS_MAKEFILE)))

# Set CURRENT_PATH to the relative path from TOP to the current directory, ie examples/device/cdc_msc_freertos
CURRENT_PATH = $(subst $(TOP)/,,$(abspath .))
