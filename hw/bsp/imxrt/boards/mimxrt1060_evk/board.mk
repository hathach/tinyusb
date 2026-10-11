CFLAGS += -DCPU_MIMXRT1062DVL6A
MCU_FAMILY = RT1060
MCU_VARIANT = MIMXRT1062

# For flash-jlink target
JLINK_DEVICE = MIMXRT1062xxx6A

# For flash-pyocd target
PYOCD_TARGET = mimxrt1060

RHPORT_DEVICE ?= 0
RHPORT_HOST ?= 1

# flash using pyocd
flash: flash-pyocd
