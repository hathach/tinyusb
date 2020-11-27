$(BUILD)/$(BOARD)-firmware.zip: $(BUILD)/$(BOARD)-firmware.hex
	adafruit-nrfutil dfu genpkg --dev-type 0x0052 --sd-req 0xFFFE --application $^ $@

# flash using adafruit-nrfutil dfu
flash-adafruit-nrfutil: $(BUILD)/$(BOARD)-firmware.zip
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	adafruit-nrfutil --verbose dfu serial --package $^ -p $(SERIAL) -b 115200 --singlebank --touch 1200
