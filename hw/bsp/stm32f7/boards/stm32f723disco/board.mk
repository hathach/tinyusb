PORT ?= 1
SPEED ?= high

CFLAGS += \
  -DSTM32F723xx \
  -DHSE_VALUE=25000000 \

# GCC
LD_FILE_GCC = $(BOARD_PATH)/STM32F723xE_FLASH.ld
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f723xx.s

# IAR
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f723xx.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f723xx_flash.icf

# flash target using on-board stlink
flash: flash-stlink

# For flash-jlink target
JLINK_DEVICE = stm32f723ie
