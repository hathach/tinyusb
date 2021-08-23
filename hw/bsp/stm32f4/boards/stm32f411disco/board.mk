CFLAGS += -DSTM32F411xE

LD_FILE = $(BOARD_PATH)/STM32F411VETx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f411xe.s

# For flash-jlink target
JLINK_DEVICE = stm32f411ve

# flash target using on-board stlink
flash: flash-stlink
