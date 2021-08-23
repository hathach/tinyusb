DEPS_SUBMODULES += hw/mcu/nxp/lpcopen

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib \
  -DCORE_M4 \
  -D__USE_LPCOPEN \
  -DCFG_TUSB_MCU=OPT_MCU_LPC43XX

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=strict-prototypes

MCU_DIR = hw/mcu/nxp/lpcopen/lpc43xx/lpc_chip_43xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/lpc4357.ld

SRC_C += \
	src/portable/nxp/transdimension/dcd_transdimension.c \
	$(MCU_DIR)/../gcc/cr_startup_lpc43xx.c \
	$(MCU_DIR)/src/chip_18xx_43xx.c \
	$(MCU_DIR)/src/clock_18xx_43xx.c \
	$(MCU_DIR)/src/gpio_18xx_43xx.c \
	$(MCU_DIR)/src/sysinit_18xx_43xx.c \
	$(MCU_DIR)/src/i2c_18xx_43xx.c \
	$(MCU_DIR)/src/i2cm_18xx_43xx.c \
	$(MCU_DIR)/src/uart_18xx_43xx.c \
	$(MCU_DIR)/src/fpu_init.c

INC += \
	$(TOP)/$(MCU_DIR)/inc \
	$(TOP)/$(MCU_DIR)/inc/config_43xx

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = LPC4357_M4

# flash using jlink
flash: flash-jlink
