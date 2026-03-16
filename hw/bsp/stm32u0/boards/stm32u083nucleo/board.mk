MCU_VARIANT = stm32u083xx
CFLAGS += \
  -DSTM32U083xx

# For flash-jlink target
JLINK_DEVICE = STM32U083RC

# flash target using on-board stlink
flash: flash-stlink
