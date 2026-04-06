CFLAGS += \
  -DSTM32L476xx \

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l476xx.s
LD_FILE = $(BOARD_PATH)/STM32L476VGTx_FLASH.ld


# For flash-jlink target
JLINK_DEVICE = stm32l476vg
