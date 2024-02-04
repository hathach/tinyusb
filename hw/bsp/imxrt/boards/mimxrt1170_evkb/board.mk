CFLAGS += -DCPU_MIMXRT1176DVMAA_cm7
MCU_VARIANT = MIMXRT1176
MCU_CORE = _cm7

# For flash-jlink target
JLINK_DEVICE = MIMXRT1176xxxA_M7

# For flash-pyocd target
PYOCD_TARGET = mimxrt1170_cm7

BOARD_TUD_RHPORT = 0
BOARD_TUH_RHPORT = 1

# flash using pyocd
flash: flash-pyocd
