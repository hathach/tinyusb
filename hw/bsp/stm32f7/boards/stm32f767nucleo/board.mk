PORT ?= 0
SPEED ?= full 

CFLAGS += \
  -DSTM32F767xx \
	-DHSE_VALUE=8000000 \

# GCC
GCC_LD_FILE = $(BOARD_PATH)/STM32F767ZITx_FLASH.ld
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f767xx.s

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f767xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f767xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f767zi

# flash target using on-board stlink
flash: flash-stlink
