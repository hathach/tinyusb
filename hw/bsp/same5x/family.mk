DEPS_SUBMODULES += hw/mcu/microchip

SDK_DIR = hw/mcu/microchip/$(MCU)
include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m4

CFLAGS += \
  -mthumb \
  -mlong-calls \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_SAME5X

# SAM driver is flooded with -Wcast-qual which slow down complication significantly
CFLAGS_SKIP += -Wcast-qual

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

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

# flash using edbg from https://github.com/ataradov/edbg
flash-edbg: $(BUILD)/$(PROJECT).bin
	edbg --verbose -t $(MCU) -pv -f $<

flash: flash-edbg
