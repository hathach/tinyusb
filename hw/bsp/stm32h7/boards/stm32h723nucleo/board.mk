CFLAGS += -DSTM32H723xx -DHSE_VALUE=8000000

# Default is FulSpeed port
PORT ?= 0

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h723xx.s
GCC_LD_FILE = $(BOARD_PATH)/stm32h723xx_flash.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32h723xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32h723xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32h723zg

# flash target using on-board stlink
flash: flash-stlink
