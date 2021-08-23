CFLAGS += -DSTM32H743xx -DHSE_VALUE=8000000

# Default is FulSpeed port
PORT ?= 0

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h743xx.s
LD_FILE = $(BOARD_PATH)/stm32h743xx_flash.ld

# For flash-jlink target
JLINK_DEVICE = stm32h743zi

# flash target using on-board stlink
flash: flash-stlink
