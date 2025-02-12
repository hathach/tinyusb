# ---------------------------------------
# Common make definition for all examples
# ---------------------------------------

#-------------- TOP and CURRENT_PATH ------------

# Set TOP to be the path to get from the current directory (where make was
# invoked) to the top of the tree. $(lastword $(MAKEFILE_LIST)) returns
# the name of this makefile relative to where make was invoked.
THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# strip off /tools/top.mk to get for example ../../..
# and Set TOP to an absolute path
TOP = $(abspath $(subst make.mk,../..,$(THIS_MAKEFILE)))

# Set CURRENT_PATH to the relative path from TOP to the current directory, ie examples/device/cdc_msc_freertos
CURRENT_PATH = $(subst $(TOP)/,,$(abspath .))

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

# Build directory
BUILD := _build
PROJECT := $(notdir $(CURDIR))

# Handy check parameter function
check_defined = \
    $(strip $(foreach 1,$1, \
    $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
    $(error Undefined make flag: $1$(if $2, ($2))))

#-------------- Fuzz harness compiler  ------------

CC ?= clang
CXX ?= clang++
GDB ?= gdb
OBJCOPY = objcopy
SIZE = size
MKDIR = mkdir

ifeq ($(CMDEXE),1)
  CP = copy
  RM = del
  PYTHON = python
else
  SED = sed
  CP = cp
  RM = rm
  PYTHON = python3
endif

#-------------- Fuzz harness flags ------------
COVERAGE_FLAGS ?= -fsanitize-coverage=trace-pc-guard
SANITIZER_FLAGS ?= -fsanitize=fuzzer \
                   -fsanitize=address

CFLAGS += $(COVERAGE_FLAGS) $(SANITIZER_FLAGS)

#-------------- Source files and compiler flags --------------
INC += $(TOP)/test

# Compiler Flags
CFLAGS += \
  -ggdb \
  -fdata-sections \
  -ffunction-sections \
  -fno-strict-aliasing \
  -Wall \
  -Wextra \
  -Werror \
  -Wfatal-errors \
  -Wdouble-promotion \
  -Wstrict-prototypes \
  -Wstrict-overflow \
  -Werror-implicit-function-declaration \
  -Wfloat-equal \
  -Wundef \
  -Wshadow \
  -Wwrite-strings \
  -Wsign-compare \
  -Wmissing-format-attribute \
  -Wunreachable-code \
  -Wcast-align \
  -Wcast-qual \
  -Wnull-dereference \
  -Wuninitialized \
  -Wunused \
  -Wredundant-decls \
  -O1

CFLAGS += \
	-Wno-error=unreachable-code \
  -DOPT_MCU_FUZZ=1 \
  -DCFG_TUSB_MCU=OPT_MCU_FUZZ \
  -D_FUZZ

CXXFLAGS += \
  -xc++ \
  -Wno-c++11-narrowing \
  -fno-implicit-templates

# conversion is too strict for most mcu driver, may be disable sign/int/arith-conversion
#  -Wconversion

# Debugging/Optimization
ifeq ($(DEBUG), 1)
  CFLAGS += -Og
else
  CFLAGS += $(CFLAGS_OPTIMIZED)
endif

# Log level is mapped to TUSB DEBUG option
ifneq ($(LOG),)
  CMAKE_DEFSYM +=	-DLOG=$(LOG)
  CFLAGS += -DCFG_TUSB_DEBUG=$(LOG)
endif

# Logger: default is uart, can be set to rtt or swo
ifneq ($(LOGGER),)
	CMAKE_DEFSYM +=	-DLOGGER=$(LOGGER)
endif
