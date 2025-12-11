UF2_FAMILY_ID = 0x2abc77ec

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m33
MCUX_DIR = /hw/mcu/nxp/mcuxsdk-core
SDK_DIR = /hw/mcu/nxp/mcux-devices-lpc

# Default to Highspeed PORT1
PORT ?= 1

CFLAGS += \
  -flto \
  -D__STARTUP_CLEAR_BSS \
  -DCFG_TUSB_MCU=OPT_MCU_LPC55XX \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' \
  -DBOARD_TUD_RHPORT=$(PORT) \

ifeq ($(PORT), 1)
  $(info "PORT1 High Speed")
  CFLAGS += -DBOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED

  # LPC55 Highspeed Port1 can only write to USB_SRAM region
  CFLAGS += -DCFG_TUSB_MEM_SECTION='__attribute__((section("m_usb_global")))'
else
  $(info "PORT0 Full Speed")
endif

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=float-equal

LDFLAGS_GCC += \
  -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \
  -Wl,--defsym=__stack_size__=0x1000 \
  -Wl,--defsym=__heap_size__=0 \

# All source paths should be relative to the top level.
LD_FILE ?= $(SDK_DIR)/LPC5500/$(MCU_VARIANT)/gcc/$(MCU_CORE)_flash.ld

SRC_C += \
	$(TOP)/src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c \
	$(TOP)/$(SDK_DIR)/LPC5500/$(MCU_VARIANT)/system_$(MCU_CORE).c \
	$(TOP)/$(SDK_DIR)/LPC5500/$(MCU_VARIANT)/drivers/fsl_clock.c \
	$(TOP)/$(SDK_DIR)/LPC5500/$(MCU_VARIANT)/drivers/fsl_power.c \
	$(TOP)/$(SDK_DIR)/LPC5500/$(MCU_VARIANT)/drivers/fsl_reset.c \
	$(TOP)/$(MCUX_DIR)/drivers/lpc_gpio/fsl_gpio.c \
	$(TOP)/$(MCUX_DIR)/drivers/common/fsl_common_arm.c \
	$(TOP)/$(MCUX_DIR)/drivers/flexcomm/fsl_flexcomm.c \
	$(TOP)/$(MCUX_DIR)/drivers/flexcomm/usart/fsl_usart.c \
	$(TOP)/lib/sct_neopixel/sct_neopixel.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/sct_neopixel \
	$(TOP)/lib/CMSIS_6/CMSIS/Core/Include \
	$(TOP)/$(SDK_DIR)/LPC5500/$(MCU_VARIANT) \
	$(TOP)/$(SDK_DIR)/LPC5500/$(MCU_VARIANT)/drivers \
	$(TOP)/$(SDK_DIR)/LPC5500/periph \
	$(TOP)/$(MCUX_DIR)/drivers/common \
	$(TOP)/$(MCUX_DIR)/drivers/flexcomm/usart \
	$(TOP)/$(MCUX_DIR)/drivers/flexcomm/ \
	$(TOP)/$(MCUX_DIR)/drivers/lpc_iocon \
	$(TOP)/$(MCUX_DIR)/drivers/lpc_gpio \
	$(TOP)/$(MCUX_DIR)/drivers/sctimer

SRC_S += $(TOP)/$(SDK_DIR)/LPC5500/$(MCU_VARIANT)/gcc/startup_$(MCU_CORE).S
