CFLAGS += \
  -DSTM32U575xx \

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/STM32U575AIIXQ_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32u575xx.s

# For flash-jlink target
JLINK_DEVICE = stm32u575
