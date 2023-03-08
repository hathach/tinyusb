CFLAGS += \
  -DHSE_VALUE=8000000 \
  -DSTM32L4R5xx \

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l4r5xx.s
GCC_LD_FILE = $(BOARD_PATH)/STM32L4RXxI_FLASH.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32l4r5xx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32l4r5xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32l4r5zi

# flash target using on-board stlink
flash: flash-stlink
