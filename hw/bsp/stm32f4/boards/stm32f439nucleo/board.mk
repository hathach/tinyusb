CFLAGS += -DSTM32F439xx

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f439xx.s
GCC_LD_FILE = $(BOARD_PATH)/STM32F439ZITX_FLASH.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f439xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f439xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f439zi

# flash target using on-board stlink
flash: flash-stlink
