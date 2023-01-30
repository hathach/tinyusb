CFLAGS += -DSTM32H743xx -DHSE_VALUE=25000000

# Default is Highspeed port
PORT ?= 1
SPEED ?= high

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h743xx.s
GCC_LD_FILE = $(BOARD_PATH)/stm32h743xx_flash.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32h743xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32h743xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32h743xi

# flash target using on-board stlink
flash: flash-stlink
