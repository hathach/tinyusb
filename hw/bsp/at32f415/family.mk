UF2_FAMILY_ID = 0x6a1b09b4
DEPS_SUBMODULES += $(FW_LIBRARY)

FW_LIBRARY = hw/mcu/arterytek/at32f415_fw_lib

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m4

PORT ?= 0

# --------------
# Compiler Flags
# --------------
CFLAGS += \
  -DCFG_TUSB_MCU=OPT_MCU_AT32F415
  -DBOARD_TUD_RHPORT=$(PORT)

# GCC Flags
CFLAGS_GCC += \
  -flto \
  -nostdlib -nostartfiles \

# mcu driver cause following warnings
CFLAGS_GCC += -Wno-error=cast-align

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

# ------------------------
# All source paths should be relative to the top level.
# ------------------------
SRC_C += \
  src/portable/synopsys/dwc2/dcd_dwc2.c \
  $(FW_LIBRARY)/libraries/cmsis/cm4/device_support/system_at32f415.c \
  $(FW_LIBRARY)/libraries/drivers/src/at32f415_crm.c \
  $(FW_LIBRARY)/libraries/drivers/src/at32f415_gpio.c \
  $(FW_LIBRARY)/libraries/drivers/src/at32f415_usart.c \
  $(FW_LIBRARY)/libraries/drivers/src/at32f415_misc.c

INC += \
  $(TOP)/$(BOARD_PATH) \
  $(TOP)/$(FW_LIBRARY)/libraries/cmsis/cm4/core_support \
  $(TOP)/$(FW_LIBRARY)/libraries/cmsis/cm4/device_support \
  $(TOP)/$(FW_LIBRARY)/libraries/drivers/inc

# Startup
SRC_S_GCC += $(FW_LIBRARY)/libraries/cmsis/cm4/device_support/startup/gcc/startup_at32f415.s

# flash target using on-board
flash: flash-jlink
