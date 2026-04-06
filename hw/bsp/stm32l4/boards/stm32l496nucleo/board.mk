CFLAGS += \
  -DSTM32L496xx \

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l496xx.s
LD_FILE = $(BOARD_PATH)/STM32L496ZGTX_FLASH.ld


# For flash-jlink target
JLINK_DEVICE = stm32l496zg
