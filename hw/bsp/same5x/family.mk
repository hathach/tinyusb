DEPS_SUBMODULES += hw/mcu/microchip

SDK_DIR = hw/mcu/microchip/$(MCU)
include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mlong-calls \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_SAME5X

# SAM driver is flooded with -Wcast-qual which slow down complication significantly
CFLAGS_SKIP += -Wcast-qual

SRC_C += \
  src/portable/microchip/samd/dcd_samd.c \
  $(SDK_DIR)/gcc/gcc/startup_$(MCU).c \
  $(SDK_DIR)/gcc/system_$(MCU).c \
  $(SDK_DIR)/hal/utils/src/utils_syscalls.c

INC += \
	$(TOP)/$(SDK_DIR) \
	$(TOP)/$(SDK_DIR)/config \
	$(TOP)/$(SDK_DIR)/include \
	$(TOP)/$(SDK_DIR)/hal/include \
	$(TOP)/$(SDK_DIR)/hal/utils/include \
	$(TOP)/$(SDK_DIR)/hpl/port \
	$(TOP)/$(SDK_DIR)/hri \
	$(TOP)/$(SDK_DIR)/CMSIS/Include

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4F

# flash using edbg from https://github.com/ataradov/edbg
flash-edbg: $(BUILD)/$(PROJECT).bin
	edbg --verbose -t $(MCU) -pv -f $<

flash: flash-edbg
