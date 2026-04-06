CFLAGS += \
  -DSTM32L412xx \

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l412xx.s
LD_FILE = $(BOARD_PATH)/STM32L412KBUx_FLASH.ld


# For flash-jlink target
JLINK_DEVICE = stm32l412kb
