CFLAGS += \
  -DSTM32L4P5xx \

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l4p5xx.s
GCC_LD_FILE = $(BOARD_PATH)/STM32L4P5ZGTX_FLASH.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32l4p5xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32l4p5xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32l4p5zg
