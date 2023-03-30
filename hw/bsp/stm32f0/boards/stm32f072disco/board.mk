CFLAGS += -DSTM32F072xB -DCFG_EXAMPLE_VIDEO_READONLY

GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f072xb.s
GCC_LD_FILE = $(BOARD_PATH)/STM32F072RBTx_FLASH.ld

IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f072xb.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f072xb_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f072rb

# flash target using on-board stlink
flash: flash-stlink
