CFLAGS += -DSTM32F412Zx

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f412zx.s
LD_FILE = $(BOARD_PATH)/STM32F412ZGTx_FLASH.ld


# For flash-jlink target
JLINK_DEVICE = stm32f412zg

# flash target using on-board stlink
flash: flash-stlink
