CFLAGS += -DCPU_MIMXRT1024DAG5A
MCU_VARIANT = MIMXRT1024

# For flash-jlink target
JLINK_DEVICE = MIMXRT1024DAG5A

# For flash-pyocd target
PYOCD_TARGET = mimxrt1024

# flash using pyocd
flash: flash-pyocd
