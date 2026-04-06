CFLAGS += -DSTM32F407xx

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f407xx.s
LD_FILE = $(BOARD_PATH)/STM32F407VETx_FLASH.ld



# For flash-jlink target
JLINK_DEVICE = stm32f407vg

# flash target using on-board stlink
flash: flash-stlink
