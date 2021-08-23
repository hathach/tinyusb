CFLAGS += -DSTM32F072xB -DLSI_VALUE=40000

LD_FILE = $(BOARD_PATH)/STM32F072VBTx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f072xb.s

# For flash-jlink target
JLINK_DEVICE = stm32f072vb

# flash target using on-board stlink
flash: flash-stlink
