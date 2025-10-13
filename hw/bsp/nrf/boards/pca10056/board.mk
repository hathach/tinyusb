MCU_VARIANT = nrf52840
CFLAGS += -DNRF52840_XXAA

# flash using jlink
flash: flash-jlink
