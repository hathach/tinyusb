MCU_VARIANT = stm32f746xx

PORT ?= 1
SPEED ?= high

CFLAGS += \
  -DSTM32F746xx \
  -DHSE_VALUE=25000000

# Linker
LD_FILE_GCC = $(BOARD_PATH)/STM32F746ZGTx_FLASH.ld

# flash target using on-board stlink
flash: flash-stlink
