CFLAGS += \
	-DSTM32C071xx

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32c071xx.s
LD_FILE = $(BOARD_PATH)/STM32C071RBTx_FLASH.ld


# For flash-jlink target
JLINK_DEVICE = stm32c071rb
