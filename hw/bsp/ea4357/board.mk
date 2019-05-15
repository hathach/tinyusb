CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -nostdlib \
  -DCORE_M4 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC43XX \
  -D__USE_LPCOPEN

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/ea4357/lpc4357.ld

# TODO remove later
SRC_C += src/portable/$(VENDOR)/$(CHIP_FAMILY)/hal_$(CHIP_FAMILY).c

SRC_C += \
	hw/mcu/nxp/lpcopen/lpc_chip_43xx/src/chip_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_43xx/src/clock_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_43xx/src/gpio_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_43xx/src/sysinit_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_43xx/src/i2c_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_43xx/src/i2cm_18xx_43xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_43xx/src/uart_18xx_43xx.c

INC += \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc_chip_43xx/inc \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc_chip_43xx/inc/config_43xx

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc18_43

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4

# For flash-jlink target
JLINK_DEVICE = LPC4357
JLINK_IF = jtag 

# flash using jlink
flash: flash-jlink
