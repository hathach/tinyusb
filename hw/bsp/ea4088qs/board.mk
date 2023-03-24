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
  -DCFG_TUD_MEM_SECTION='__attribute__((section(".data.$$RAM2")))' \
  -DCFG_TUH_MEM_SECTION='__attribute__((section(".data.$$RAM2")))' \
  -DCFG_TUSB_MCU=OPT_MCU_LPC40XX

# mcu driver cause following warnings
CFLAGS += -Wno-error=strict-prototypes -Wno-error=unused-parameter -Wno-error=cast-qual

MCU_DIR = hw/mcu/nxp/lpcopen/lpc40xx/lpc_chip_40xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/lpc4088.ld

SRC_C += \
	src/portable/nxp/lpc17_40/dcd_lpc17_40.c \
	$(MCU_DIR)/../gcc/cr_startup_lpc40xx.c \
	$(MCU_DIR)/src/chip_17xx_40xx.c \
	$(MCU_DIR)/src/clock_17xx_40xx.c \
	$(MCU_DIR)/src/gpio_17xx_40xx.c \
	$(MCU_DIR)/src/iocon_17xx_40xx.c \
	$(MCU_DIR)/src/sysctl_17xx_40xx.c \
	$(MCU_DIR)/src/sysinit_17xx_40xx.c \
	$(MCU_DIR)/src/uart_17xx_40xx.c \
	$(MCU_DIR)/src/fpu_init.c

INC += \
	$(TOP)/$(MCU_DIR)/inc

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = LPC4088

# flash using jlink
flash: flash-jlink
