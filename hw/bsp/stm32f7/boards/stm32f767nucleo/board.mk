PORT ?= 0
SPEED ?= full

CFLAGS += \
  -DSTM32F767xx \
	-DHSE_VALUE=8000000 \

# GCC
LD_FILE_GCC = $(BOARD_PATH)/STM32F767ZITx_FLASH.ld
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f767xx.s

# IAR
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f767xx.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f767xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f767zi

# flash target using on-board stlink
flash: flash-stlink
