CFLAGS += -DSTM32F405xx

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f405xx.s
GCC_LD_FILE = $(BOARD_PATH)/STM32F405RGTx_FLASH.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f405xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f405xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f405rg

# flash target using on-board stlink
flash: flash-stlink
