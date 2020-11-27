LD_FILE ?= hw/bsp/nrf/boards/$(BOARD)/$(BOARD).ld

# flash using Nordic nrfutil (pip3 install nrfutil)
# 	make BOARD=nrf52840_mdk_dongle SERIAL=/dev/ttyACM0 all flash
NRFUTIL = nrfutil

$(BUILD)/$(BOARD)-firmware-nrfutil.zip: $(BUILD)/$(BOARD)-firmware.hex
	$(NRFUTIL) pkg generate --hw-version 52 --sd-req 0x0000 --debug-mode --application $^ $@

flash: $(BUILD)/$(BOARD)-firmware-nrfutil.zip
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(NRFUTIL) dfu usb-serial --package $^ -p $(SERIAL) -b 115200