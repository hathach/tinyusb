MCU = same54

CFLAGS += \
  -DCONF_CPU_FREQUENCY=48000000 \
  -D__SAME54P20A__ \
  -DBOARD_NAME="\"Microchip SAM E54 Xplained Pro\""

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/same54p20a_flash.ld

# For flash-jlink target
JLINK_DEVICE = ATSAME54P20
