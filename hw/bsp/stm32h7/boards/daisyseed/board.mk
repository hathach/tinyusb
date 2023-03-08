CFLAGS += -DSTM32H750xx -DCORE_CM7 -DHSE_VALUE=16000000

# Default is FulSpeed port
PORT ?= 0

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h750xx.s
GCC_LD_FILE = $(BOARD_PATH)/stm32h750ibkx_flash.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32h750xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32h750xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32h750ibk6_m7

# flash target using on-board stlink
flash: flash-stlink

