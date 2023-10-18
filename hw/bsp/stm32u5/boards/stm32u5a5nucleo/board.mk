CFLAGS += \
  -DSTM32U5A5xx \

# All source paths should be relative to the top level.
LD_FILE = ${BOARD_PATH}/STM32U5A5ZJTXQ_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32u5a5xx.s

# For flash-jlink target
JLINK_DEVICE = stm32u575zi
