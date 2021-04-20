CFLAGS += -DSTM32F070xB

LD_FILE = $(BOARD_PATH)/stm32F070rbtx_flash.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f070xb.s

# For flash-jlink target
JLINK_DEVICE = stm32f070rb

# flash target using on-board stlink
flash: flash-stlink
