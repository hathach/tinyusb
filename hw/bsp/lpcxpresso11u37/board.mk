CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0 \
  -nostdlib \
  -DCORE_M0 \
  -D__USE_LPCOPEN \
  -DCFG_TUSB_MCU=OPT_MCU_LPC11UXX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM2")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

# startup.c and lpc_types.h cause following errors
CFLAGS += -Wno-error=nested-externs -Wno-error=strict-prototypes

MCU_DIR = hw/mcu/nxp/lpc_driver/lpc11uxx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/lpcxpresso11u37/lpc11u37.ld

SRC_C += \
	$(MCU_DIR)/cr_startup_lpc11xx.c \
	$(MCU_DIR)/lpc_chip_11uxx/src/chip_11xx.c \
	$(MCU_DIR)/lpc_chip_11uxx/src/clock_11xx.c \
	$(MCU_DIR)/lpc_chip_11uxx/src/gpio_11xx_1.c \
	$(MCU_DIR)/lpc_chip_11uxx/src/iocon_11xx.c \
	$(MCU_DIR)/lpc_chip_11uxx/src/sysctl_11xx.c \
	$(MCU_DIR)/lpc_chip_11uxx/src/sysinit_11xx.c

INC += \
	$(TOP)/$(MCU_DIR)/lpc_chip_11uxx/inc

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
