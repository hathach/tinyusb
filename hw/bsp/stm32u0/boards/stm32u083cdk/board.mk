MCU_VARIANT = stm32u083xx
CFLAGS += \
  -DSTM32U083xx

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/STM32U083MCTx_FLASH.ld
LD_FILE_IAR = $(BOARD_PATH)/stm32u083xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = STM32U083MC

# flash target using on-board stlink
flash: flash-stlink
