CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -nostdlib \
  -DCORE_M0PLUS \
  -D__VTOR_PRESENT=0 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC11UXX \
  -D__USE_LPCOPEN \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM3")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/lpcxpresso11u68/lpc11u68.ld

SRC_C += \
	hw/mcu/nxp/lpcopen/lpc_chip_11u6x/src/chip_11u6x.c \
	hw/mcu/nxp/lpcopen/lpc_chip_11u6x/src/clock_11u6x.c \
	hw/mcu/nxp/lpcopen/lpc_chip_11u6x/src/gpio_11u6x.c \
	hw/mcu/nxp/lpcopen/lpc_chip_11u6x/src/iocon_11u6x.c \
	hw/mcu/nxp/lpcopen/lpc_chip_11u6x/src/syscon_11u6x.c \
	hw/mcu/nxp/lpcopen/lpc_chip_11u6x/src/sysinit_11u6x.c

INC += \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc_chip_11u6x/inc

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc11_13_15

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = LPC11U68
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
