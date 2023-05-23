UF2_FAMILY_ID = 0x2abc77ec
SDK_DIR = hw/mcu/nxp/mcux-sdk

DEPS_SUBMODULES += $(SDK_DIR) lib/CMSIS_5

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m33
include $(TOP)/tools/make/cpu/$(CPU_CORE).mk

# Default to Highspeed PORT1
PORT ?= 1

CFLAGS += \
  -flto \
  -DCFG_TUSB_MCU=OPT_MCU_MCXN9 \
  -DBOARD_TUD_RHPORT=$(PORT) \

ifeq ($(PORT), 1)
  $(info "PORT1 High Speed")
  CFLAGS += -DBOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
else
  $(info "PORT0 Full Speed")
endif

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=old-style-declaration

# All source paths should be relative to the top level.
LD_FILE ?= $(SDK_DIR)/devices/$(MCU_VARIANT)/gcc/$(MCU_CORE)_flash.ld

SRC_C += \
	src/portable/chipidea/ci_hs/dcd_ci_hs.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/system_$(MCU_CORE).c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_clock.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_reset.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_gpio.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_common_arm.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_lpflexcomm.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_lpuart.c \

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(SDK_DIR)/devices/$(MCU_VARIANT) \
	$(TOP)/$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers \

SRC_S += $(SDK_DIR)/devices/$(MCU_VARIANT)/gcc/startup_$(MCU_CORE).S
