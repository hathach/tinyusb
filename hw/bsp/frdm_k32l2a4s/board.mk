
# Default to a less-verbose build.  If you want all the gory compiler output,
# "VERBOSE=1" to the make command line.
ifndef VERBOSE
.SILENT:
$(info Non-Verbose Output)
else
$(info Verbose Output)
endif

SDK_DIR = hw/mcu/nxp/mcux-sdk
MCU_DIR = $(SDK_DIR)/devices/K32L2A4S

ifdef VERBOSE
$(info TOP='$(TOP)')
$(info )

$(info BSP='$(TOP)/hw/bsp/$(BOARD)')
$(info )

$(info TOP/SDK_DIR='$(TOP)/$(SDK_DIR)')
$(info )

$(info MCU_DIR='$(MCU_DIR)')
$(info )
endif

DEPS_SUBMODULES += $(SDK_DIR)

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -DCPU_K32L2A41VLH1A \
  -DCFG_TUSB_MCU=OPT_MCU_K32L2AXX

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=redundant-decls -Wno-error=cast-qual

# All source paths should be relative to the top level.

LD_FILE  = $(MCU_DIR)/gcc/frdmk32l2a4s.ld
LDFLAGS += -L$(TOP)/$(MCU_DIR)/gcc

# Define Recursive Depth wildcard:
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

SRC_C += \
	src/portable/nxp/khci/dcd_khci.c \
	$(MCU_DIR)/gcc/startup_k32l2a41a.c

SRC_C += $(call rwildcard,$(TOP)/$(MCU_DIR)/config,*.c)
SRC_C += $(call rwildcard,$(TOP)/$(MCU_DIR)/drivers,*.c)

INC += \
	$(TOP)/hw/bsp/$(BOARD) \
	$(TOP)/$(MCU_DIR)/CMSIS/ \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/config \
	$(TOP)/$(MCU_DIR)/drivers

ifdef VERBOSE
$(info INC = '$(strip $(INC))')
$(info )

$(info SRC_C = '$(sort $(strip $(SRC_C)))')
$(info )
endif

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
#JLINK_DEVICE = ?

# For flash-pyocd target
PYOCD_TARGET = K32L2A

# flash using pyocd
flash: flash-pyocd
