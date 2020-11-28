CFLAGS += -D__SAMD21G18A__

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/feather_m0_express.ld

# For flash-jlink target
JLINK_DEVICE = ATSAMD21G18

# flash using bossac at least version 1.8
# can be found in arduino15/packages/arduino/tools/bossac/
# Add it to your PATH or change BOSSAC variable to match your installation
BOSSAC = bossac

flash: $(BUILD)/$(BOARD)-firmware.bin
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(BOSSAC) --port=$(SERIAL) -U -i --offset=0x2000 -e -w $^ -R
