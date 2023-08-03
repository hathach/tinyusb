MCU_VARIANT = stm32f746xx

PORT ?= 0
SPEED ?= full

CFLAGS += \
  -DSTM32F746xx \
  -DHSE_VALUE=8000000

# Linker
LD_FILE_GCC = $(BOARD_PATH)/STM32F746ZGTx_FLASH.ld

# flash target using on-board stlink
flash: flash-stlink
