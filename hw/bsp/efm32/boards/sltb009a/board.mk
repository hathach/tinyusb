EFM32_FAMILY = efm32gg12b
EFM32_MCU = EFM32GG12B810F1024GM64
JLINK_DEVICE = $(EFM32_MCU)

CFLAGS += \
  -D$(EFM32_MCU) \

# For flash-jlink target
flash: flash-jlink
