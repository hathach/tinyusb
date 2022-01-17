CFLAGS += -DSTM32F439xx

LD_FILE = $(BOARD_PATH)/STM32F439ZITX_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f439xx.s

# For flash-jlink target
JLINK_DEVICE = stm32f439zi

# flash target using on-board stlink
flash: flash-stlink
