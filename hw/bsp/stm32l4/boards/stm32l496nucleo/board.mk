CFLAGS += \
  -DSTM32L496xx \

# GCC
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l496xx.s
LD_FILE_GCC = $(BOARD_PATH)/STM32L496ZGTX_FLASH.ld

# IAR
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32l496xx.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32l496xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32l496zg
