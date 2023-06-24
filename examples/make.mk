# ---------------------------------------
# Common make definition for all examples
# ---------------------------------------

# Supported toolchain: gcc, iar
TOOLCHAIN ?= gcc

#-------------- TOP and CURRENT_PATH ------------

# Set TOP to be the path to get from the current directory (where make was
# invoked) to the top of the tree. $(lastword $(MAKEFILE_LIST)) returns
# the name of this makefile relative to where make was invoked.
THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# strip off /tools/top.mk to get for example ../../..
# and Set TOP to an absolute path
TOP = $(abspath $(subst make.mk,..,$(THIS_MAKEFILE)))

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

# Handy check parameter function
check_defined = \
    $(strip $(foreach 1,$1, \
    $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
    $(error Undefined make flag: $1$(if $2, ($2))))


# Build directory
BUILD := _build/$(BOARD)

PROJECT := $(notdir $(CURDIR))
BIN := $(TOP)/_bin/$(BOARD)/$(notdir $(CURDIR))

#-------------- Select the board to build for. ------------

# Board without family
ifneq ($(wildcard $(TOP)/hw/bsp/$(BOARD)/board.mk),)
BOARD_PATH := hw/bsp/$(BOARD)
FAMILY :=
endif

# Board within family
ifeq ($(BOARD_PATH),)
  BOARD_PATH := $(subst $(TOP)/,,$(wildcard $(TOP)/hw/bsp/*/boards/$(BOARD)))
  FAMILY := $(word 3, $(subst /, ,$(BOARD_PATH)))
  FAMILY_PATH = hw/bsp/$(FAMILY)
endif

ifeq ($(BOARD_PATH),)
  $(info You must provide a BOARD parameter with 'BOARD=')
  $(error Invalid BOARD specified)
endif

ifeq ($(FAMILY),)
  include $(TOP)/hw/bsp/$(BOARD)/board.mk
else
  # Include Family and Board specific defs
  include $(TOP)/$(FAMILY_PATH)/family.mk

  SRC_C += $(subst $(TOP)/,,$(wildcard $(TOP)/$(FAMILY_PATH)/*.c))
endif

#-------------- Cross Compiler  ------------

# Can be set by board, default to ARM GCC
CROSS_COMPILE ?= arm-none-eabi-

ifeq ($(TOOLCHAIN),iar)
CC := iccarm
endif

ifeq ($(CC),iccarm)
USE_IAR = 1
endif

ifeq ($(CMDEXE),1)
  CP = copy
  RM = del
  MKDIR = mkdir
  PYTHON = python
else
  CP = cp
  RM = rm
  MKDIR = mkdir
  PYTHON = python3
endif

#-------------- Source files and compiler flags --------------
# tinyusb makefile
include $(TOP)/src/tinyusb.mk

# Include all source C in family & board folder
SRC_C += hw/bsp/board.c
SRC_C += $(subst $(TOP)/,,$(wildcard $(TOP)/$(BOARD_PATH)/*.c))

SRC_C += $(TINYUSB_SRC_C)

INC += \
  $(TOP)/$(FAMILY_PATH) \
  $(TOP)/src \


# Log level is mapped to TUSB DEBUG option
ifneq ($(LOG),)
  CMAKE_DEFSYM +=	-DLOG=$(LOG)
  CFLAGS += -DCFG_TUSB_DEBUG=$(LOG)
endif

# Logger: default is uart, can be set to rtt or swo
ifneq ($(LOGGER),)
	CMAKE_DEFSYM +=	-DLOGGER=$(LOGGER)
endif

ifeq ($(LOGGER),rtt)
  CFLAGS += -DLOGGER_RTT -DSEGGER_RTT_MODE_DEFAULT=SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL
  RTT_SRC = lib/SEGGER_RTT
  INC   += $(TOP)/$(RTT_SRC)/RTT
  SRC_C += $(RTT_SRC)/RTT/SEGGER_RTT.c
else ifeq ($(LOGGER),swo)
  CFLAGS += -DLOGGER_SWO
endif

# CPU specific flags
ifdef CPU_CORE
include $(TOP)/tools/make/cpu/$(CPU_CORE).mk
endif

# toolchain specific
include $(TOP)/tools/make/toolchain/arm_$(TOOLCHAIN).mk
