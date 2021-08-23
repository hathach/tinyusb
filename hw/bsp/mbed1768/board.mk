DEPS_SUBMODULES += hw/mcu/nxp/lpcopen

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -nostdlib \
  -DCORE_M3 \
  -D__USE_LPCOPEN \
  -DCFG_TUSB_MCU=OPT_MCU_LPC175X_6X \
  -DRTC_EV_SUPPORT=0

# startup.c and lpc_types.h cause following errors
CFLAGS += -Wno-error=strict-prototypes

MCU_DIR = hw/mcu/nxp/lpcopen/lpc175x_6x/lpc_chip_175x_6x

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/lpc1768.ld

SRC_C += \
	src/portable/nxp/lpc17_40/dcd_lpc17_40.c \
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

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = LPC1768

# flash using pyocd 
flash: $(BUILD)/$(PROJECT).hex
	pyocd flash -t lpc1768 $<

