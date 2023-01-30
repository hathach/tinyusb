CFLAGS += -DSTM32H723xx -DHSE_VALUE=8000000

# Default is FulSpeed port
PORT ?= 0

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h723xx.s
LD_FILE = $(BOARD_PATH)/stm32h723xx_flash.ld

# For flash-jlink target
JLINK_DEVICE = stm32h723zg

# flash target using on-board stlink
flash: flash-stlink
