CFLAGS += -DCPU_MIMXRT1064DVL6A
MCU_VARIANT = MIMXRT1064

# For flash-jlink target
JLINK_DEVICE = MIMXRT1064xxx6A

# For flash-pyocd target
PYOCD_TARGET = mimxrt1064

# flash using pyocd
flash: flash-pyocd
