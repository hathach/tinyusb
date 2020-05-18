CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -D__SAMG55J19__ \
  -DCFG_TUSB_MCU=OPT_MCU_SAMG

# suppress following warnings from mcu driver
CFLAGS += -Wno-error=undef

ASF_DIR = hw/mcu/microchip/samg55

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/samg55j19_flash.ld

SRC_C += \
	$(ASF_DIR)/samg55/gcc/gcc/startup_samg55j19.c \
	$(ASF_DIR)/samg55/gcc/system_samg55j19.c \
	$(ASF_DIR)/hpl/core/hpl_init.c \
	$(ASF_DIR)/hpl/usart/hpl_usart.c \
	$(ASF_DIR)/hpl/pmc/hpl_pmc.c \
	$(ASF_DIR)/hal/src/hal_atomic.c

INC += \
  $(TOP)/hw/bsp/$(BOARD) \
	$(TOP)/$(ASF_DIR) \
	$(TOP)/$(ASF_DIR)/config \
	$(TOP)/$(ASF_DIR)/samg55/include \
	$(TOP)/$(ASF_DIR)/hal/include \
	$(TOP)/$(ASF_DIR)/hal/utils/include \
	$(TOP)/$(ASF_DIR)/hpl/core \
	$(TOP)/$(ASF_DIR)/hpl/pio \
	$(TOP)/$(ASF_DIR)/hpl/pmc \
	$(TOP)/$(ASF_DIR)/hri \
	$(TOP)/$(ASF_DIR)/CMSIS/Core/Include

# For TinyUSB port source
VENDOR = microchip
CHIP_FAMILY = samg

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = ATSAMD51J19
JLINK_IF = swd

# flash using edbg from https://github.com/ataradov/edbg
flash: $(BUILD)/$(BOARD)-firmware.bin
	edbg --verbose -t samg55 -pv -f $< 
