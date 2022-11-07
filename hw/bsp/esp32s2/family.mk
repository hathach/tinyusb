#DEPS_SUBMODULES +=

.PHONY: all clean flash bootloader-flash app-flash erase monitor dfu-flash dfu

all:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) -DIDF_TARGET=esp32s2 build

build: all

fullclean:
	if test -f sdkconfig; then $(RM) -f sdkconfig ; fi
	if test -d $(BUILD); then $(RM) -rf $(BUILD) ; fi
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) $@

clean flash bootloader-flash app-flash erase monitor dfu-flash dfu size size-components size-files:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) $@

uf2: $(BUILD)/$(PROJECT).uf2

UF2_FAMILY_ID = 0xbfdd4eee
$(BUILD)/$(PROJECT).uf2: $(BUILD)/$(PROJECT).bin
	@echo CREATE $@
	$(PYTHON) $(TOP)/tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY_ID) -b 0x0 -c -o $@ $^

