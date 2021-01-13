PORT ?= 1
SPEED ?= high

CFLAGS += \
  -DSTM32F723xx \
  -DHSE_VALUE=25000000 \

LD_FILE = $(BOARD_PATH)/STM32F723xE_FLASH.ld
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f723xx.s

# flash target using on-board stlink
flash: flash-stlink
