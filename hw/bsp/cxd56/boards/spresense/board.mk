# Spresense board configuration
SERIAL ?= /dev/ttyUSB0

# flash
flash: $(BUILD)/$(PROJECT).spk
	@echo FLASH $<
	@$(PYTHON) $(TOP)/hw/mcu/sony/cxd56/tools/flash_writer.py -s -c $(SERIAL) -d -b 115200 -n $<
