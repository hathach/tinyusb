CFLAGS += \
  -DHSE_VALUE=8000000 \
  -DSTM32L4R5xx \

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/STM32L4RXxI_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l4r5xx.s

# For flash-jlink target
JLINK_DEVICE = stm32l4r5zi

# flash target using on-board stlink
flash: flash-stlink
