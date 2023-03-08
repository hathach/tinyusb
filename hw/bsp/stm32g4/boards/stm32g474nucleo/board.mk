CFLAGS += \
	-DSTM32G474xx \
	-DHSE_VALUE=24000000

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32g474xx.s
GCC_LD_FILE = $(BOARD_PATH)/STM32G474RETx_FLASH.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32g474xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32g474xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32g474re
