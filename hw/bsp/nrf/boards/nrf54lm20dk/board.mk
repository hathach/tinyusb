MCU_VARIANT = nrf54lm20a_enga
CFLAGS += -DNRF54LM20A_ENGA_XXAA

# flash using jlink
JLINK_DEVICE = NRF54LM20A_M33
flash: flash-jlink
