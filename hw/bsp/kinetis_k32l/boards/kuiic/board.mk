MCU_VARIANT = K32L2B31A

CFLAGS += -DCPU_K32L2B31VLH0A

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=redundant-decls

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/kuiic.ld

INC += \
	$(TOP)/$(MCUX_DEVICES)/K32L/periph2

SRC_C += \
	$(BOARD_PATH)/clock_config.c

# For flash-jlink target
JLINK_DEVICE = K32L2B31xxxxA

# For flash-pyocd target
PYOCD_TARGET = K32L2B

# flash using pyocd
flash: flash-pyocd
