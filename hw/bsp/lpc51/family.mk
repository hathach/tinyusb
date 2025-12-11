include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m0plus
MCUX_DIR = /hw/mcu/nxp/mcuxsdk-core
SDK_DIR = /hw/mcu/nxp/mcux-devices-lpc

CFLAGS += \
  -flto \
  -D__STARTUP_CLEAR_BSS \
  -DCFG_TUSB_MCU=OPT_MCU_LPC51UXX \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))'

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter

LDFLAGS_GCC += \
  -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \

# All source paths should be relative to the top level.
LD_FILE = $(SDK_DIR)/LPC51U68/$(MCU_VARIANT)/gcc/$(MCU_VARIANT)_flash.ld

SRC_C += \
	src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c \
	$(TOP)/$(SDK_DIR)/LPC51U68/$(MCU_VARIANT)/system_$(MCU_VARIANT).c \
	$(TOP)/$(SDK_DIR)/LPC51U68/$(MCU_VARIANT)/drivers/fsl_clock.c \
	$(TOP)/$(SDK_DIR)/LPC51U68/$(MCU_VARIANT)/drivers/fsl_power.c \
	$(TOP)/$(SDK_DIR)/LPC51U68/$(MCU_VARIANT)/drivers/fsl_reset.c \
	$(TOP)/$(MCUX_DIR)/drivers/lpc_gpio/fsl_gpio.c \
	$(TOP)/$(MCUX_DIR)/drivers/flexcomm/fsl_flexcomm.c \
	$(TOP)/$(MCUX_DIR)/drivers/flexcomm/usart/fsl_usart.c \

INC += \
    $(TOP)/$(BOARD_PATH) \
    $(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(SDK_DIR)/LPC51U68/$(MCU_VARIANT) \
	$(TOP)/$(SDK_DIR)/LPC51U68/$(MCU_VARIANT)/drivers \
	$(TOP)/$(SDK_DIR)/LPC51U68/periph \
	$(TOP)/$(MCUX_DIR)/drivers/common \
	$(TOP)/$(MCUX_DIR)/drivers/flexcomm \
	$(TOP)/$(MCUX_DIR)/drivers/flexcomm/usart \
	$(TOP)/$(MCUX_DIR)/drivers/lpc_iocon \
	$(TOP)/$(MCUX_DIR)/drivers/lpc_gpio

SRC_S += $(TOP)$(SDK_DIR)/LPC51U68/$(MCU_VARIANT)/gcc/startup_$(MCU_VARIANT).S
