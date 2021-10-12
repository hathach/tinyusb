DEPS_SUBMODULES += hw/mcu/nxp/lpcopen

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0 \
  -nostdlib \
  -DCORE_M0 \
  -D__USE_LPCOPEN \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_EXAMPLE_VIDEO_READONLY \
  -DCFG_TUSB_MCU=OPT_MCU_LPC11UXX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM2")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

# mcu driver cause following warnings
CFLAGS += -Wno-error=strict-prototypes -Wno-error=unused-parameter

MCU_DIR = hw/mcu/nxp/lpcopen/lpc11uxx/lpc_chip_11uxx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/lpc11u37.ld

SRC_C += \
	src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c \
	$(MCU_DIR)/../gcc/cr_startup_lpc11xx.c \
	$(MCU_DIR)/src/chip_11xx.c \
	$(MCU_DIR)/src/clock_11xx.c \
	$(MCU_DIR)/src/gpio_11xx_1.c \
	$(MCU_DIR)/src/iocon_11xx.c \
	$(MCU_DIR)/src/sysctl_11xx.c \
	$(MCU_DIR)/src/sysinit_11xx.c

INC += \
	$(TOP)/$(MCU_DIR)/inc

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = LPC11U37/401

# flash using pyocd 
flash: $(BUILD)/$(PROJECT).hex
	pyocd flash -t lpc11u37 $<
