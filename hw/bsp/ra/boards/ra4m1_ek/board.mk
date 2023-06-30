CPU_CORE = cortex-m4
MCU_VARIANT = ra4m1

FSP_BOARD_DIR = hw/mcu/renesas/fsp/ra/board/ra4m1_ek

SRC_C += \
	$(FSP_BOARD_DIR)/board_init.c \
	$(FSP_BOARD_DIR)/board_leds.c \

INC += \
	$(TOP)/$(FSP_BOARD_DIR)

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/ra4m1_ek.ld

# For flash-jlink target
JLINK_DEVICE = R7FA4M1AB
JLINK_IF     = SWD

flash: flash-jlink
