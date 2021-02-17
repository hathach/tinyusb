CFLAGS += -DSTM32F412Zx

LD_FILE = $(BOARD_PATH)/STM32F412ZGTx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f412zx.s

# For flash-jlink target
JLINK_DEVICE = stm32f412zg

# flash target using on-board stlink
flash: flash-stlink
