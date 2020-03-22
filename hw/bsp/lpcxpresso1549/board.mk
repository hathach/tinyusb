CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -nostdlib \
  -DCORE_M3 \
  -D__USE_LPCOPEN \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_TUSB_MCU=OPT_MCU_LPC15XX \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

# mcu driver cause following warnings
CFLAGS += -Wno-error=strict-prototypes -Wno-error=unused-parameter -Wno-error=unused-variable

MCU_DIR = hw/mcu/nxp/lpcopen/lpc15xx/lpc_chip_15xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/lpc1549.ld

SRC_C += \
	$(MCU_DIR)/../gcc/cr_startup_lpc15xx.c \
	$(MCU_DIR)/src/chip_15xx.c \
	$(MCU_DIR)/src/clock_15xx.c \
	$(MCU_DIR)/src/gpio_15xx.c \
	$(MCU_DIR)/src/iocon_15xx.c \
	$(MCU_DIR)/src/swm_15xx.c \
	$(MCU_DIR)/src/sysctl_15xx.c \
	$(MCU_DIR)/src/sysinit_15xx.c

INC += \
	$(TOP)/$(MCU_DIR)/inc

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc_ip3511

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = LPC1549
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
