CFLAGS += -DSTM32H750xx -DCORE_CM7 -DHSE_VALUE=16000000

# Default is FulSpeed port
PORT ?= 0

# GCC
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h750xx.s
LD_FILE_GCC = $(BOARD_PATH)/stm32h750ibkx_flash.ld

# IAR
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32h750xx.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32h750xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32h750ibk6_m7

# flash target using on-board stlink
flash: flash-stlink
