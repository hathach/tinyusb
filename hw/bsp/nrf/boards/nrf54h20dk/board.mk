MCU_VARIANT = nrf54h20
CFLAGS += -DNRF54H20_XXAA

# 32 KB primary RAM is too tight for memory-heavy examples (e.g. video YUY2
# framebuf). Match the CMake build (board.cmake) — TODO: route static .bss to
# RAM00 (512 KB) and drop this.
CFLAGS += -DCFG_EXAMPLE_VIDEO_READONLY

# enable max3421 host driver for this board
MAX3421_HOST = 1

# caused by void SystemStoreFICRNS() (without void) in system_nrf5340_application.c
CFLAGS += -Wno-error=strict-prototypes

# flash using jlink
JLINK_DEVICE = nrf5340_xxaa_app
flash: flash-jlink
