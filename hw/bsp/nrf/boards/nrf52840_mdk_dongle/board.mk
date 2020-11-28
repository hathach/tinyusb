MCU_VARIANT = nrf52840
CFLAGS += -DNRF52840_XXAA

LD_FILE = $(BOARD_PATH)/$(BOARD).ld

# flash using Nordic nrfutil (pip3 install nrfutil)
# 	make BOARD=nrf52840_mdk_dongle SERIAL=/dev/ttyACM0 all flash
NRFUTIL = nrfutil

$(BUILD)/$(BOARD)-firmware.zip: $(BUILD)/$(BOARD)-firmware.hex
	$(NRFUTIL) pkg generate --hw-version 52 --sd-req 0x0000 --debug-mode --application $^ $@

flash: $(BUILD)/$(BOARD)-firmware.zip
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(NRFUTIL) dfu usb-serial --package $^ -p $(SERIAL) -b 115200