#
# Common make definition for all examples
#

# Compiler 
CROSS_COMPILE = arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size
MKDIR = mkdir
SED = sed
CP = cp
RM = rm
PYTHON ?= python

check_defined = \
    $(strip $(foreach 1,$1, \
    $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
    $(error Undefined make flag: $1$(if $2, ($2))))

# Select the board to build for.
ifeq ($(BOARD),)
  $(info You must provide a BOARD parameter with 'BOARD=')
  $(info Possible values are:)
  $(info $(sort $(subst /.,,$(subst $(TOP)/hw/bsp/,,$(wildcard $(TOP)/hw/bsp/*/.)))))
  $(error BOARD not defined)
else
  ifeq ($(wildcard $(TOP)/hw/bsp/$(BOARD)/.),)
    $(error Invalid BOARD specified)
  endif
endif

# Build directory
BUILD = _build/build-$(BOARD)

# Board specific
include $(TOP)/hw/bsp/$(BOARD)/board.mk

# Include all source C in board folder
SRC_C += $(subst $(TOP)/,,$(wildcard $(TOP)/hw/bsp/$(BOARD)/*.c))

# Compiler Flags
CFLAGS += \
	-fsingle-precision-constant \
	-fno-strict-aliasing \
	-Wdouble-promotion \
	-Wno-endif-labels \
	-Wstrict-prototypes \
	-Wall \
	-Werror \
	-Werror-implicit-function-declaration \
	-Wfloat-equal \
	-Wundef \
	-Wshadow \
	-Wwrite-strings \
	-Wsign-compare \
	-Wmissing-format-attribute \
	-Wno-deprecated-declarations \
	-Wnested-externs \
	-Wunreachable-code \
	-Wno-error=lto-type-mismatch \
	-ffunction-sections \
	-fdata-sections

# This causes lots of warning with nrf5x build due to nrfx code
# CFLAGS += -Wcast-align

# Debugging/Optimization
ifeq ($(DEBUG), 1)
  CFLAGS += -O0 -ggdb -DCFG_TUSB_DEBUG=1
else
  CFLAGS += -flto -Os
endif
