MCU_VARIANT = nrf52833
CFLAGS += -DNRF52833_XXAA

LD_FILE = ${FAMILY_PATH}/linker/nrf52833_xxaa.ld

# flash using jlink
flash: flash-jlink
