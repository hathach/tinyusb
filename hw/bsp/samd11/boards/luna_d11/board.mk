CFLAGS += -D__SAMD11D14AM__

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/samd11d14am_flash.ld

# For flash-jlink target
JLINK_DEVICE = ATSAMD11D14

# flash using dfu-util
flash: $(BUILD)/$(PROJECT).bin
	dfu-util -a 0 -d 1d50:615c -D $< || dfu-util -a 0 -d 16d0:05a5 -D $<
