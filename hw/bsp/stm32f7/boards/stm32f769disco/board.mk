# Only OTG-HS has a connector on this board
PORT ?= 1
SPEED ?= high

CFLAGS += \
  -DSTM32F769xx \
  -DHSE_VALUE=25000000 \

# GCC
GCC_LD_FILE = $(BOARD_PATH)/STM32F769ZITx_FLASH.ld
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f769xx.s

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f769xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f769xx_flash.icf

# flash target using on-board stlink
flash: flash-stlink
