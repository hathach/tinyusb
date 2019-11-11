CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0 \
  -nostdlib \
  -DCORE_M0 \
  -D__USE_LPCOPEN \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_TUSB_MCU=OPT_MCU_LPC11UXX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM2")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

# mcu driver cause following warnings
CFLAGS += -Wno-error=nested-externs -Wno-error=strict-prototypes -Wno-error=unused-parameter

MCU_DIR = hw/mcu/nxp/lpcopen/lpc11uxx/lpc_chip_11uxx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/lpcxpresso11u37/lpc11u37.ld

SRC_C += \
	$(MCU_DIR)/../gcc/cr_startup_lpc11xx.c \
	$(MCU_DIR)/src/chip_11xx.c \
	$(MCU_DIR)/src/clock_11xx.c \
	$(MCU_DIR)/src/gpio_11xx_1.c \
	$(MCU_DIR)/src/iocon_11xx.c \
	$(MCU_DIR)/src/sysctl_11xx.c \
	$(MCU_DIR)/src/sysinit_11xx.c

INC += \
	$(TOP)/$(MCU_DIR)/inc

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc_ip3511

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = LPC11U37/401
JLINK_IF = swd

# flash using pyocd 
flash: $(BUILD)/$(BOARD)-firmware.hex
	pyocd flash -t lpc11u37 $<
