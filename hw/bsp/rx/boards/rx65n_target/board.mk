CFLAGS += \
  -mcpu=rx64m \
  -misa=v2 \
  -DCFG_TUSB_MCU=OPT_MCU_RX65X

MCU_DIR = hw/mcu/renesas/rx/rx65n

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/r5f565ne.ld

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RX600

# For flash-jlink target
JLINK_DEVICE = R5F565NE
JLINK_IF     = JTAG

# For flash-pyocd target
PYOCD_TARGET =

# flash using rfp-cli
flash: flash-rfp
