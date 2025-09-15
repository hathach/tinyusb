MCU_VARIANT = stm32l053xx
CFLAGS += \
  -DSTM32L053xx

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/STM32L053C8Tx_FLASH.ld

# For flash-jlink target
JLINK_DEVICE = STM32L053R8

# flash target using on-board stlink
flash: flash-stlink
