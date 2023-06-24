PORT ?= 0
SPEED ?= full

CFLAGS += \
  -DSTM32F746xx \
  -DHSE_VALUE=8000000

# GCC
LD_FILE_GCC = $(BOARD_PATH)/STM32F746ZGTx_FLASH.ld
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f746xx.s

# IAR
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f746xx.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f746xx_flash.icf

# flash target using on-board stlink
flash: flash-stlink
