# ---------------------------------------
# Common make definition for all examples
# ---------------------------------------

#-------------------------------------------------------------
# Toolchain
# Can be changed via TOOLCHAIN=gcc|iar or CC=arm-none-eabi-gcc|iccarm|clang
#-------------------------------------------------------------

ifneq (,$(findstring clang,$(CC)))
  TOOLCHAIN = clang
else ifneq (,$(findstring iccarm,$(CC)))
  TOOLCHAIN = iar
else ifneq (,$(findstring gcc,$(CC)))
  TOOLCHAIN = gcc
endif

# Default to GCC
ifndef TOOLCHAIN
  TOOLCHAIN = gcc
endif

#-------------- TOP and CURRENT_PATH ------------

# Set TOP to be the path to get from the current directory (where make was invoked) to the top of the tree.
# $(lastword $(MAKEFILE_LIST)) returns the name of this makefile relative to where make was invoked.
THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# strip off /examples/build_system/make to get for example ../../..
# and Set TOP to an absolute path
TOP = $(abspath $(subst make.mk,../../..,$(THIS_MAKEFILE)))

# Set CURRENT_PATH to the relative path from TOP to the current directory, ie examples/device/cdc_msc_freertos
CURRENT_PATH = $(subst $(TOP)/,,$(abspath .))

#-------------- Linux/Windows ------------

# Detect whether shell style is windows or not
# https://stackoverflow.com/questions/714100/os-detecting-makefile/52062069#52062069
ifeq '$(findstring ;,$(PATH))' ';'
# PATH contains semicolon - so we're definitely on Windows.
CMDEXE := 1

# makefile shell commands should use syntax for DOS CMD, not unix sh
# Force DOS command shell on Windows.
SHELL := cmd.exe
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

#-------------- Source files and compiler flags --------------
# tinyusb makefile
include $(TOP)/src/tinyusb.mk
SRC_C += $(TINYUSB_SRC_C)

# Include all source C in family & board folder
SRC_C += hw/bsp/board.c
SRC_C += $(subst $(TOP)/,,$(wildcard $(TOP)/$(BOARD_PATH)/*.c))

INC += \
  $(TOP)/$(FAMILY_PATH) \
  $(TOP)/src \

BOARD_UPPER = $(subst a,A,$(subst b,B,$(subst c,C,$(subst d,D,$(subst e,E,$(subst f,F,$(subst g,G,$(subst h,H,$(subst i,I,$(subst j,J,$(subst k,K,$(subst l,L,$(subst m,M,$(subst n,N,$(subst o,O,$(subst p,P,$(subst q,Q,$(subst r,R,$(subst s,S,$(subst t,T,$(subst u,U,$(subst v,V,$(subst w,W,$(subst x,X,$(subst y,Y,$(subst z,Z,$(subst -,_,$(BOARD))))))))))))))))))))))))))))
CFLAGS += -DBOARD_$(BOARD_UPPER)

ifdef CFLAGS_CLI
	CFLAGS += $(CFLAGS_CLI)
endif

# use max3421 as host controller
ifeq (${MAX3421_HOST},1)
  SRC_C += src/portable/analog/max3421/hcd_max3421.c
  CFLAGS += -DCFG_TUH_MAX3421=1
  CMAKE_DEFSYM +=	-DMAX3421_HOST=1
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
  include ${TOP}/examples/build_system/make/cpu/$(CPU_CORE).mk
endif

# toolchain specific
include ${TOP}/examples/build_system/make/toolchain/arm_$(TOOLCHAIN).mk

# Handy check parameter function
check_defined = \
    $(strip $(foreach 1,$1, \
    $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
    $(error Undefined make flag: $1$(if $2, ($2))))
