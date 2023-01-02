CFLAGS += \
  -DSTM32L412xx \

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/STM32L412KBUx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l412xx.s

# For flash-jlink target
JLINK_DEVICE = stm32l412kb
