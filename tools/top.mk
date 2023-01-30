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

#$(info top.mk: SHELL=$(SHELL))
#$(info top.mk: CMDEXE=$(CMDEXE))

# Set TOP to be the path to get from the current directory (where make was
# invoked) to the top of the tree. $(lastword $(MAKEFILE_LIST)) returns
# the name of this makefile relative to where make was invoked.
THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# strip off /tools/top.mk to get for example ../../..
TOP := $(patsubst %/tools/top.mk,%,$(THIS_MAKEFILE))
#$(info top.mk: Initial TOP=$(TOP))

# Set TOP to an absolute path, for example /tinyUSB (from ../../..)
ifeq ($(CMDEXE),1)
TOP := $(subst \,/,$(shell for %%i in ( $(TOP) ) do echo %%~fi))
else
TOP := $(shell realpath $(TOP))
endif
#$(info top.mk: Top directory is $(TOP))

# Set CURRENT_PATH to the relative path from TOP to the current directory, ie examples/device/cdc_msc_freertos
ifeq ($(CMDEXE),1)
CURRENT_PATH := $(subst $(TOP)/,,$(subst \,/,$(shell echo %CD%)))
else
CURRENT_PATH := $(shell realpath --relative-to=$(TOP) `pwd`)
endif
#$(info top.mk: Path from top is $(CURRENT_PATH))
