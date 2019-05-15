CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -nostdlib \
  -DCORE_M3 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC13XX \
  -D__USE_LPCOPEN \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM3")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/lpcxpresso1347/lpc1347.ld

SRC_C += \
	hw/mcu/nxp/lpcopen/lpc_chip_13xx/src/chip_13xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_13xx/src/clock_13xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_13xx/src/gpio_13xx_1.c \
	hw/mcu/nxp/lpcopen/lpc_chip_13xx/src/iocon_13xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_13xx/src/sysctl_13xx.c \
	hw/mcu/nxp/lpcopen/lpc_chip_13xx/src/sysinit_13xx.c

INC += \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc_chip_13xx/inc

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc11_13_15

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = LPC1347
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
