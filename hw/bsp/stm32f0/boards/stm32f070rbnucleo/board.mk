CFLAGS += -DSTM32F070xB -DCFG_EXAMPLE_VIDEO_READONLY

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f070xb.s
GCC_LD_FILE = $(BOARD_PATH)/stm32F070rbtx_flash.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f070xb.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f070xb_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f070rb

# flash target using on-board stlink
flash: flash-stlink