CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -nostdlib \
  -DCORE_M3 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC18XX \
  -D__USE_LPCOPEN

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/mcb1800/lpc1857.ld

# TODO remove later
SRC_C += src/portable/$(VENDOR)/$(CHIP_FAMILY)/hal_$(CHIP_FAMILY).c

SRC_C += \
	hw/mcu/nxp/lpcopen/lpc_chip_18xx/src/chip_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_18xx/src/clock_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_18xx/src/gpio_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_18xx/src/sysinit_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_18xx/src/uart_18xx_43xx.c

INC += \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc_chip_18xx/inc \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc_chip_18xx/inc/config_18xx

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc18_43

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = LPC1857
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
