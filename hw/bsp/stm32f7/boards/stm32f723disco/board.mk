MCU_VARIANT = stm32f723xx

RHPORT_SPEED = OPT_MODE_FULL_SPEED OPT_MODE_HIGH_SPEED
RHPORT_DEVICE ?= 1
RHPORT_HOST ?= 0

CFLAGS += \
  -DSTM32F723xx \
  -DHSE_VALUE=25000000 \

# Linker
LD_FILE_GCC = $(BOARD_PATH)/STM32F723xE_FLASH.ld

# flash target using on-board stlink
flash: flash-stlink

# For flash-jlink target
JLINK_DEVICE = stm32f723ie
