MCU = MK28FA15

CFLAGS += -DCPU_MK28FN2M0AVMI15

# mcu driver cause following warnings
CFLAGS += -Wno-unused-parameter -Wno-redundant-decls

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/MK28FN2M0Axxx15_flash.ld

# For flash-jlink target
JLINK_DEVICE = MK28FN2M0Axxx15

# For flash-pyocd target
PYOCD_TARGET = MK28FA15

# flash using pyocd
flash: flash-pyocd
