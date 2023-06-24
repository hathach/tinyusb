CFLAGS += -DSTM32F072xB -DLSI_VALUE=40000 -DCFG_EXAMPLE_VIDEO_READONLY

SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f072xb.s
LD_FILE_GCC = $(BOARD_PATH)/STM32F072VBTx_FLASH.ld

SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f072xb.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f072xb_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f072vb

# flash target using on-board stlink
flash: flash-stlink
