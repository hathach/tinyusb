CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -nostdlib \
  -DCORE_M3 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC18XX \
  -D__USE_LPCOPEN

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=strict-prototypes

MCU_DIR = hw/mcu/nxp/lpcopen/lpc18xx/lpc_chip_18xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/lpc1857.ld

SRC_C += \
	$(MCU_DIR)/../gcc/cr_startup_lpc18xx.c \
	$(MCU_DIR)/src/chip_18xx_43xx.c \
	$(MCU_DIR)/src/clock_18xx_43xx.c \
	$(MCU_DIR)/src/gpio_18xx_43xx.c \
	$(MCU_DIR)/src/sysinit_18xx_43xx.c \
	$(MCU_DIR)/src/uart_18xx_43xx.c

INC += \
	$(TOP)/$(MCU_DIR)/inc \
	$(TOP)/$(MCU_DIR)/inc/config_18xx

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = transdimension

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = LPC1857
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
