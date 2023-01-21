CFLAGS += -DSTM32F072xB -DLSI_VALUE=40000 -DCFG_EXAMPLE_VIDEO_READONLY

GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f072xb.s
GCC_LD_FILE = $(BOARD_PATH)/STM32F072VBTx_FLASH.ld

IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f072xb.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f072xb_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f072vb

# flash target using on-board stlink
flash: flash-stlink
