MCU_VARIANT = nrf54h20
CFLAGS += -DNRF54H20_XXAA

# enable max3421 host driver for this board
MAX3421_HOST = 1

# caused by void SystemStoreFICRNS() (without void) in system_nrf5340_application.c
CFLAGS += -Wno-error=strict-prototypes

# flash using jlink
JLINK_DEVICE = nrf5340_xxaa_app
flash: flash-jlink
