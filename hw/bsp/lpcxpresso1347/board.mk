CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -nostdlib \
  -DCORE_M3 \
  -D__USE_LPCOPEN \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_TUSB_MCU=OPT_MCU_LPC13XX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM2")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

# startup.c and lpc_types.h cause following errors
CFLAGS += -Wno-error=nested-externs -Wno-error=strict-prototypes

MCU_DIR = hw/mcu/nxp/lpcopen/lpc13xx/lpc_chip_13xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/lpcxpresso1347/lpc1347.ld

SRC_C += \
	$(MCU_DIR)/../gcc/cr_startup_lpc13xx.c \
	$(MCU_DIR)/src/chip_13xx.c \
	$(MCU_DIR)/src/clock_13xx.c \
	$(MCU_DIR)/src/gpio_13xx_1.c \
	$(MCU_DIR)/src/iocon_13xx.c \
	$(MCU_DIR)/src/sysctl_13xx.c \
	$(MCU_DIR)/src/sysinit_13xx.c

INC += \
	$(TOP)/$(MCU_DIR)/inc

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc_ip3511

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = LPC1347
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
