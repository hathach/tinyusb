CFLAGS += -DCPU_MIMXRT1064DVL6A
MCU_FAMILY = RT1064
MCU_VARIANT = MIMXRT1064

# For flash-jlink target
JLINK_DEVICE = MIMXRT1064xxx6A

# For flash-pyocd target
PYOCD_TARGET = mimxrt1064

RHPORT_DEVICE ?= 0
RHPORT_HOST ?= 1

# flash using pyocd
flash: flash-pyocd
