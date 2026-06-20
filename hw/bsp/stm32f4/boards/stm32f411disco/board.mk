CFLAGS += -DSTM32F411xE

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f411xe.s
LD_FILE = $(BOARD_PATH)/STM32F411VETx_FLASH.ld


# For flash-jlink target
JLINK_DEVICE = stm32f411ve

# flash target using on-board stlink
flash: flash-stlink
