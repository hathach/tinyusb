PORT ?= 1
SPEED ?= high

CFLAGS += \
  -DSTM32F746xx \
  -DHSE_VALUE=25000000

# GCC
GCC_LD_FILE = $(BOARD_PATH)/STM32F746ZGTx_FLASH.ld
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f746xx.s

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f746xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f746xx_flash.icf

# flash target using on-board stlink
flash: flash-stlink
