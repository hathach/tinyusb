ifneq ($(lastword a b),b)
$(error This Makefile require make 3.81 or newer)
endif

# Set TOP to be the path to get from the current directory (where make was
# invoked) to the top of the tree. $(lastword $(MAKEFILE_LIST)) returns
# the name of this makefile relative to where make was invoked.

THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))
TOP := $(patsubst %/tools/top.mk,%,$(THIS_MAKEFILE))

TOP := $(shell realpath $(TOP))

#$(info Top directory is $(TOP))

CURRENT_PATH := $(shell realpath --relative-to=$(TOP) `pwd`)
#$(info Path from top is $(CURRENT_PATH))
