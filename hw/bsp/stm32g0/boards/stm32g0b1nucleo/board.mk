CFLAGS += \
	-DSTM32G0B1xx

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32g0b1xx.s
GCC_LD_FILE = $(BOARD_PATH)/STM32G0B1RETx_FLASH.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32g0b1xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32g0b1xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32g0b1re
