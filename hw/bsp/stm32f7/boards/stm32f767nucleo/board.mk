PORT ?= 0
SPEED ?= full 

CFLAGS += \
  -DSTM32F767xx \
	-DHSE_VALUE=8000000 \

LD_FILE = $(BOARD_PATH)/STM32F767ZITx_FLASH.ld
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f767xx.s

# For flash-jlink target
JLINK_DEVICE = stm32f767zi

# flash target using on-board stlink
flash: flash-stlink
