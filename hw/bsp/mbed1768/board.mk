CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -nostdlib \
  -DCORE_M3 \
  -D__USE_LPCOPEN \
  -DCFG_TUSB_MCU=OPT_MCU_LPC175X_6X \
  -DRTC_EV_SUPPORT=0

# startup.c and lpc_types.h cause following errors
CFLAGS += -Wno-error=nested-externs -Wno-error=strict-prototypes

MCU_DIR = hw/mcu/nxp/lpcopen/lpc175x_6x/lpc_chip_175x_6x

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/mbed1768/lpc1768.ld

# TODO remove later
SRC_C += src/portable/$(VENDOR)/$(CHIP_FAMILY)/hal_$(CHIP_FAMILY).c

SRC_C += \
	$(MCU_DIR)/../gcc/cr_startup_lpc175x_6x.c \
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
JLINK_DEVICE = LPC1768
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
