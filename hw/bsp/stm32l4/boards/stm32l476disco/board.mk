CFLAGS += \
  -DSTM32L476xx \

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/STM32L476VGTx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l476xx.s

# For flash-jlink target
JLINK_DEVICE = stm32l476vg
