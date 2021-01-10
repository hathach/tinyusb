ifneq ($(lastword a b),b)
$(error This Makefile require make 3.81 or newer)
endif

# Detect windows or not
# https://stackoverflow.com/questions/714100/os-detecting-makefile/52062069#52062069
ifeq '$(findstring ;,$(PATH))' ';'
UNAME := Windows
endif

# Set TOP to be the path to get from the current directory (where make was
# invoked) to the top of the tree. $(lastword $(MAKEFILE_LIST)) returns
# the name of this makefile relative to where make was invoked.

THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))
TOP := $(patsubst %/tools/top.mk,%,$(THIS_MAKEFILE))

ifeq ($(UNAME),Windows)
TOP := $(subst \,/,$(shell for %%i in ( $(TOP) ) do echo %%~fi))
else
TOP := $(shell realpath $(TOP))
endif
#$(info Top directory is $(TOP))

ifeq ($(UNAME),Windows)
CURRENT_PATH := $(subst $(TOP)/,,$(subst \,/,$(shell echo %CD%)))
else
CURRENT_PATH := $(shell realpath --relative-to=$(TOP) `pwd`)
endif
#$(info Path from top is $(CURRENT_PATH))
