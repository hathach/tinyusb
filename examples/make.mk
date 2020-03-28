# ---------------------------------------
# Common make definition for all examples
# ---------------------------------------

#-------------- Select the board to build for. ------------
BOARD_LIST = $(sort $(subst /.,,$(subst $(TOP)/hw/bsp/,,$(wildcard $(TOP)/hw/bsp/*/.))))

ifeq ($(filter $(BOARD),$(BOARD_LIST)),)
  $(info You must provide a BOARD parameter with 'BOARD=', supported boards are:)
  $(foreach b,$(BOARD_LIST),$(info - $(b)))
  $(error Invalid BOARD specified)
endif

# Handy check parameter function
check_defined = \
    $(strip $(foreach 1,$1, \
    $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
    $(error Undefined make flag: $1$(if $2, ($2))))
    
# Build directory
BUILD = _build/build-$(BOARD)

# Board specific define
include $(TOP)/hw/bsp/$(BOARD)/board.mk

#-------------- Cross Compiler  ------------
# Can be set by board, default to ARM GCC
CROSS_COMPILE ?= arm-none-eabi-

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size
MKDIR = mkdir
SED = sed
CP = cp
RM = rm

#-------------- Source files and compiler flags --------------

# Include all source C in board folder
SRC_C += hw/bsp/board.c
SRC_C += $(subst $(TOP)/,,$(wildcard $(TOP)/hw/bsp/$(BOARD)/*.c))

# Compiler Flags
CFLAGS += \
	-fdata-sections \
	-ffunction-sections \
	-fsingle-precision-constant \
	-fno-strict-aliasing \
	-Wdouble-promotion \
	-Wstrict-prototypes \
	-Wall \
	-Wextra \
	-Werror \
	-Wfatal-errors \
	-Werror-implicit-function-declaration \
	-Wfloat-equal \
	-Wundef \
	-Wshadow \
	-Wwrite-strings \
	-Wsign-compare \
	-Wmissing-format-attribute \
	-Wunreachable-code

# This causes lots of warning with nrf5x build due to nrfx code
# CFLAGS += -Wcast-align

# Debugging/Optimization
ifeq ($(DEBUG), 1)
  CFLAGS += -Og -ggdb
else
	CFLAGS += -Os
endif

# TUSB Logging option
ifneq ($(LOG),)
  CFLAGS += -DCFG_TUSB_DEBUG=$(LOG)
endif
