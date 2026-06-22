MCU_VARIANT = LPC55S69
MCU_CORE = LPC55S69_cm33_core0
RHPORT_DEVICE ?= 1

CFLAGS += -DCPU_LPC55S69JBD100_cm33_core0
LD_FILE = $(BOARD_PATH)/LPC55S69_cm33_core0_uf2.ld

SRC_C += \
	$(TOP)/$(BOARD_PATH)/board/clock_config.c \
	$(TOP)/$(BOARD_PATH)/board/pin_mux.c \
	$(TOP)/$(BOARD_PATH)/board/peripherals.c

INC += $(TOP)/$(BOARD_PATH)/board

JLINK_DEVICE = LPC55S69
PYOCD_TARGET = LPC55S69

# flash using pyocd
flash: flash-pyocd
