CFLAGS += -DSTM32F072xB -DCFG_EXAMPLE_VIDEO_READONLY

LD_FILE = $(BOARD_PATH)/STM32F072RBTx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f072xb.s

# For flash-jlink target
JLINK_DEVICE = stm32f072rb

# flash target using on-board stlink
flash: flash-stlink
