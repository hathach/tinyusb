UF2_FAMILY_ID = 0x2abc77ec

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m33 \
  -mfloat-abi=hard \
  -mfpu=fpv5-sp-d16 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC55XX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))'

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=float-equal

MCU_DIR = hw/mcu/nxp/sdk/devices/LPC55S69

# All source paths should be relative to the top level.
LD_FILE ?= $(MCU_DIR)/gcc/$(MCU_VARIANT)_flash.ld

SRC_C += \
	$(MCU_DIR)/system_$(MCU_VARIANT).c \
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

SRC_S += $(MCU_DIR)/gcc/startup_$(MCU_VARIANT).S

LIBS += $(TOP)/$(MCU_DIR)/gcc/libpower_hardabi.a

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc_ip3511

# For freeRTOS port source
FREERTOS_PORT = ARM_CM33_NTZ/non_secure

# For flash-jlink target
#JLINK_DEVICE = LPC55S69

# flash using pyocd
#flash: $(BUILD)/$(PROJECT).hex
#	pyocd flash -t LPC55S69 $<
