# Only OTG-HS has a connector on this board
PORT ?= 1
SPEED ?= high

CFLAGS += \
  -DSTM32F769xx \
  -DHSE_VALUE=25000000 \

LD_FILE = $(BOARD_PATH)/STM32F769ZITx_FLASH.ld
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f769xx.s

# flash target using on-board stlink
flash: flash-stlink
