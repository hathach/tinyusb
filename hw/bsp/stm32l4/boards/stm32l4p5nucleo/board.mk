CFLAGS += \
  -DSTM32L4P5xx \

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/STM32L4P5ZGTX_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l4p5xx.s

# For flash-jlink target
JLINK_DEVICE = stm32l4p5zg
