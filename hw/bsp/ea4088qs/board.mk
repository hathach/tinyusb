CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -nostdlib \
  -DCORE_M4 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC40XX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM2")))' \
  -D__USE_LPCOPEN

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/ea4088qs/lpc4088.ld

# TODO remove later
SRC_C += src/portable/$(VENDOR)/$(CHIP_FAMILY)/hal_$(CHIP_FAMILY).c

SRC_C += \
	hw/mcu/nxp/lpcopen/lpc_chip_40xx/src/chip_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_40xx/src/clock_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_40xx/src/gpio_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_40xx/src/iocon_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_40xx/src/sysctl_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_40xx/src/sysinit_17xx_40xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_40xx/src/uart_17xx_40xx.c

INC += \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc_chip_40xx/inc

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc17_40

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = LPC4088
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
