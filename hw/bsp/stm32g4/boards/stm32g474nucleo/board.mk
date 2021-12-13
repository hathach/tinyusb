CFLAGS += \
	-DSTM32G474xx \
	-DHSE_VALUE=24000000

LD_FILE = $(BOARD_PATH)/STM32G474RETx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32g474xx.s

# For flash-jlink target
JLINK_DEVICE = stm32g474re
