CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -nostdlib \
  -DCORE_M4 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC40XX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM2")))' \
  -D__USE_LPCOPEN

# mcu driver cause following warnings
CFLAGS += -Wno-error=strict-prototypes -Wno-error=unused-parameter

MCU_DIR = hw/mcu/nxp/lpcopen/lpc40xx/lpc_chip_40xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/lpc4088.ld

SRC_C += \
	$(MCU_DIR)/../gcc/cr_startup_lpc40xx.c \
	$(MCU_DIR)/src/chip_17xx_40xx.c \
	$(MCU_DIR)/src/clock_17xx_40xx.c \
	$(MCU_DIR)/src/gpio_17xx_40xx.c \
	$(MCU_DIR)/src/iocon_17xx_40xx.c \
	$(MCU_DIR)/src/sysctl_17xx_40xx.c \
	$(MCU_DIR)/src/sysinit_17xx_40xx.c \
	$(MCU_DIR)/src/uart_17xx_40xx.c

INC += \
	$(TOP)/$(MCU_DIR)/inc

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
