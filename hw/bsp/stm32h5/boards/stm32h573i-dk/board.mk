CFLAGS += \
	-DSTM32H573xx

# GCC
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h573xx.s
LD_FILE_GCC = $(BOARD_PATH)/STM32H573I-DK_FLASH.ld

# IAR
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32h573xx.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32h573xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32h573i-dk
