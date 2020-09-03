CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -nostdlib \
  -DCORE_M0PLUS \
  -D__VTOR_PRESENT=0 \
  -D__USE_LPCOPEN \
  -DCFG_TUSB_MCU=OPT_MCU_LPC11UXX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM3")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

MCU_DIR = hw/mcu/nxp/lpcopen/lpc11u6x/lpc_chip_11u6x

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/lpc11u68.ld

SRC_C += \
	$(MCU_DIR)/../gcc/cr_startup_lpc11u6x.c \
	$(MCU_DIR)/src/chip_11u6x.c \
	$(MCU_DIR)/src/clock_11u6x.c \
	$(MCU_DIR)/src/gpio_11u6x.c \
	$(MCU_DIR)/src/iocon_11u6x.c \
	$(MCU_DIR)/src/syscon_11u6x.c \
	$(MCU_DIR)/src/sysinit_11u6x.c

INC += \
	$(TOP)/$(MCU_DIR)/inc

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc_ip3511

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = LPC11U68
JLINK_IF = swd

# flash using pyocd 
flash: $(BUILD)/$(BOARD)-firmware.hex
	pyocd flash -t lpc11u68 $<
