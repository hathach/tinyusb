UF2_FAMILY_ID = 0x2abc77ec
SDK_DIR = hw/mcu/nxp/mcux-sdk

DEPS_SUBMODULES += $(SDK_DIR) lib/CMSIS_5		#not found

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m33-nodsp-nofp

CFLAGS += \
  -flto \
  -DCFG_TUSB_MCU=OPT_MCU_MCXA \
  -DBOARD_TUD_RHPORT=0 

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=old-style-declaration

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

# All source paths should be relative to the top level.
LD_FILE ?= $(SDK_DIR)/devices/$(MCU_VARIANT)/gcc/$(MCU_CORE)_flash.ld

# TinyUSB: Port0 is chipidea FS, Port1 is chipidea HS
$(info "PORT0 Full Speed")
CFLAGS += -DBOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
SRC_C += src/portable/chipidea/ci_fs/dcd_ci_fs.c

SRC_C += \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/system_$(MCU_CORE).c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_clock.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_reset.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_gpio.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_lpuart.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_common_arm.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_spc.c 
INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(SDK_DIR)/devices/$(MCU_VARIANT) \
	$(TOP)/$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers 
	

SRC_S += $(SDK_DIR)/devices/$(MCU_VARIANT)/gcc/startup_$(MCU_CORE).S
