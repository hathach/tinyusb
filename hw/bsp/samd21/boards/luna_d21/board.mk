CFLAGS += -D__SAMD21G18A__

LD_FILE = $(BOARD_PATH)/samd21g18a_flash.ld

# For flash-jlink target
JLINK_DEVICE = ATSAMD21G18

# flash using dfu-util
flash: $(BUILD)/$(PROJECT).bin
	dfu-util -a 0 -d 1d50:615c -D $< || dfu-util -a 0 -d 16d0:05a5 -D $<

