# ---------------------------------------
# Common make definition for all examples
# ---------------------------------------

# Build directory
BUILD := _build/$(BOARD)

PROJECT := $(notdir $(CURDIR))
BIN := $(TOP)/_bin/$(BOARD)/$(notdir $(CURDIR))

# Handy check parameter function
check_defined = \
    $(strip $(foreach 1,$1, \
    $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
    $(error Undefined make flag: $1$(if $2, ($2))))

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

# Allow for -Os to be changed by board makefiles in case -Os is not allowed
CFLAGS_OPTIMIZED ?= -Os

ifeq ($(CC),iccarm)
USE_IAR = 1
endif

ifdef USE_IAR
  AS = iasmarm
  LD = ilinkarm
  OBJCOPY = ielftool
  SIZE = echo "size not available for IAR"

else
  CC = $(CROSS_COMPILE)gcc
  CXX = $(CROSS_COMPILE)g++
  AS = $(CC) -x assembler-with-cpp
  LD = $(CC)
  
  GDB = $(CROSS_COMPILE)gdb
  OBJCOPY = $(CROSS_COMPILE)objcopy
  SIZE = $(CROSS_COMPILE)size
endif

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

#-------------- Source files and compiler flags --------------

# Include all source C in family & board folder
SRC_C += hw/bsp/board.c
SRC_C += $(subst $(TOP)/,,$(wildcard $(TOP)/$(BOARD_PATH)/*.c))

INC   += $(TOP)/$(FAMILY_PATH)

# GCC Compiler Flags
GCC_CFLAGS += \
  -ggdb \
  -fdata-sections \
  -ffunction-sections \
  -fsingle-precision-constant \
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
  -Wcast-function-type \
  -Wcast-qual \
  -Wnull-dereference \
  -Wuninitialized \
  -Wunused \
  -Wredundant-decls

# conversion is too strict for most mcu driver, may be disable sign/int/arith-conversion
#  -Wconversion

# Debugging/Optimization
ifeq ($(DEBUG), 1)
  GCC_CFLAGS += -O0
  NO_LTO = 1
else
  GCC_CFLAGS += $(CFLAGS_OPTIMIZED)
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
