# Only OTG-HS has a connector on this board
PORT ?= 1
SPEED ?= high

CFLAGS += \
  -DSTM32F769xx \
  -DHSE_VALUE=25000000 \

# GCC
LD_FILE_GCC = $(BOARD_PATH)/STM32F769ZITx_FLASH.ld
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f769xx.s

# IAR
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f769xx.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f769xx_flash.icf

# flash target using on-board stlink
flash: flash-stlink
