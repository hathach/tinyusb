CFLAGS += \
	-DSTM32G0B1xx

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32g0b1xx.s
LD_FILE = $(BOARD_PATH)/STM32G0B1RETx_FLASH.ld


# For flash-jlink target
JLINK_DEVICE = stm32g0b1re
