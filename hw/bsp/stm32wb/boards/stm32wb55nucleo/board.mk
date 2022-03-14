CFLAGS += \
	-DSTM32WB55xx

LD_FILE = $(BOARD_PATH)/stm32wb55xx_flash_cm4.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32wb55xx_cm4.s

# For flash-jlink target
JLINK_DEVICE = STM32WB55RG
