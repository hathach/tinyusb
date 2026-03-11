MCU_VARIANT = LPC55S28
MCU_CORE = LPC55S28
MCU_DRIVER_VARIANT = LPC55S69

# device fullspeed, host highspeed
RHPORT_DEVICE ?= 0
RHPORT_HOST ?= 1

CFLAGS += -DCPU_LPC55S28JBD100

SRC_C += \
	$(TOP)/$(BOARD_PATH)/board/clock_config.c \
	$(TOP)/$(BOARD_PATH)/board/pin_mux.c \
	$(TOP)/$(BOARD_PATH)/board/peripherals.c

INC += $(TOP)/$(BOARD_PATH)/board

JLINK_DEVICE = LPC55S28
PYOCD_TARGET = LPC55S28

# flash using pyocd
flash: flash-pyocd
