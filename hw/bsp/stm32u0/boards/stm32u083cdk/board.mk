MCU_VARIANT = stm32u083xx
CFLAGS += \
  -DSTM32U083xx

# For flash-jlink target
JLINK_DEVICE = STM32U083MC

# flash target using on-board stlink
flash: flash-stlink
