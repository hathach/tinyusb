DEPS_SUBMODULES += hw/mcu/nxp/lpcopen

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -nostdlib \
  -DCORE_M3 \
  -D__USE_LPCOPEN \
  -DCFG_TUSB_MCU=OPT_MCU_LPC18XX

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=strict-prototypes

MCU_DIR = hw/mcu/nxp/lpcopen/lpc18xx/lpc_chip_18xx

SRC_C += \
	src/portable/nxp/transdimension/dcd_transdimension.c \
	$(MCU_DIR)/../gcc/cr_startup_lpc18xx.c \
	$(MCU_DIR)/src/chip_18xx_43xx.c \
	$(MCU_DIR)/src/clock_18xx_43xx.c \
	$(MCU_DIR)/src/gpio_18xx_43xx.c \
	$(MCU_DIR)/src/sysinit_18xx_43xx.c \
	$(MCU_DIR)/src/uart_18xx_43xx.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(MCU_DIR)/inc \
	$(TOP)/$(MCU_DIR)/inc/config_18xx

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3
