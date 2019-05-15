CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -nostdlib \
  -DCORE_M3 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC175X_6X \
  -D__USE_LPCOPEN \
  -DRTC_EV_SUPPORT=0

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/lpcxpresso1769/lpc1769.ld

# TODO remove later
SRC_C += src/portable/$(VENDOR)/$(CHIP_FAMILY)/hal_$(CHIP_FAMILY).c

SRC_C += \
	hw/mcu/nxp/lpcopen/lpc_chip_175x_6x/src/chip_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_175x_6x/src/clock_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_175x_6x/src/gpio_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_175x_6x/src/iocon_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_175x_6x/src/sysctl_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_175x_6x/src/sysinit_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_175x_6x/src/uart_17xx_40xx.c

INC += \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc_chip_175x_6x/inc

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc17_40

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = LPC1769
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
