DEPS_SUBMODULES += hw/mcu/microchip

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
CFLAGS += -Wno-error=undef -Wno-error=cast-qual -Wno-error=null-dereference -Wno-error=redundant-decls

ASF_DIR = hw/mcu/microchip/samg55

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/samg55j19_flash.ld

SRC_C += \
	src/portable/microchip/samg/dcd_samg.c \
	$(ASF_DIR)/samg55/gcc/gcc/startup_samg55.c \
	$(ASF_DIR)/samg55/gcc/system_samg55.c \
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

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = ATSAMG55J19

# flash using edbg from https://github.com/ataradov/edbg
flash: $(BUILD)/$(PROJECT).bin
	edbg --verbose -t samg55 -pv -f $< 
