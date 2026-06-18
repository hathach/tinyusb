CFLAGS += \
  -DSTM32L4P5xx \

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l4p5xx.s
LD_FILE = $(BOARD_PATH)/STM32L4P5ZGTX_FLASH.ld


# For flash-jlink target
JLINK_DEVICE = stm32l4p5zg
