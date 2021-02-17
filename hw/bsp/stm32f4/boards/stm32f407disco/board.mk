CFLAGS += -DSTM32F407xx

LD_FILE = $(BOARD_PATH)/STM32F407VGTx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f407xx.s

# For flash-jlink target
JLINK_DEVICE = stm32f407vg

# flash target using on-board stlink
flash: flash-stlink
