PORT ?= 1
SPEED ?= high

CFLAGS += \
  -DSTM32F746xx \
  -DHSE_VALUE=25000000

LD_FILE = $(BOARD_PATH)/STM32F746ZGTx_FLASH.ld
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f746xx.s

# flash target using on-board stlink
flash: flash-stlink
