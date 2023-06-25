CFLAGS += -DSTM32H723xx -DHSE_VALUE=8000000

# Default is FulSpeed port
PORT ?= 0

# GCC
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h723xx.s
LD_FILE_GCC =  $(FAMILY_PATH)/linker/stm32h723xx_flash.ld

# IAR
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32h723xx.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32h723xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32h723zg

# flash target using on-board stlink
flash: flash-stlink
