UF2_FAMILY_ID = 0x2abc77ec
DEPS_SUBMODULES += lib/sct_neopixel hw/mcu/nxp

include $(TOP)/$(BOARD_PATH)/board.mk

# TODO change Default to Highspeed PORT1
PORT ?= 0

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m33 \
  -mfloat-abi=hard \
  -mfpu=fpv5-sp-d16 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC55XX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' \
  -DBOARD_DEVICE_RHPORT_NUM=$(PORT)

ifeq ($(PORT), 1)
  CFLAGS += -DBOARD_DEVICE_RHPORT_SPEED=OPT_MODE_HIGH_SPEED
  $(info "PORT1 High Speed")
else
  $(info "PORT0 Full Speed")
endif

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=float-equal

MCU_DIR = hw/mcu/nxp/sdk/devices/$(MCU_VARIANT)

# All source paths should be relative to the top level.
LD_FILE ?= $(MCU_DIR)/gcc/$(MCU_CORE)_flash.ld

SRC_C += \
	src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c \
	$(MCU_DIR)/system_$(MCU_CORE).c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_gpio.c \
	$(MCU_DIR)/drivers/fsl_power.c \
	$(MCU_DIR)/drivers/fsl_reset.c \
	$(MCU_DIR)/drivers/fsl_usart.c \
	$(MCU_DIR)/drivers/fsl_flexcomm.c \
	lib/sct_neopixel/sct_neopixel.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/sct_neopixel \
	$(TOP)/$(MCU_DIR)/../../CMSIS/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/drivers

SRC_S += $(MCU_DIR)/gcc/startup_$(MCU_CORE).S

LIBS += $(TOP)/$(MCU_DIR)/gcc/libpower_hardabi.a

# For freeRTOS port source
FREERTOS_PORT = ARM_CM33_NTZ/non_secure
