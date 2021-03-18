#DEPS_SUBMODULES +=

.PHONY: all clean flash

all:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) build

build: all

clean:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) clean

fullclean:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) fullclean

flash:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) flash

bootloader-flash:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) bootloader-flash

app-flash:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) app-flash

erase:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) erase_flash

monitor:
	idf.py -B$(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) $(CMAKE_DEFSYM) monitor

uf2: $(BUILD)/$(PROJECT).uf2

UF2_FAMILY_ID = 0xbfdd4eee
$(BUILD)/$(PROJECT).uf2: $(BUILD)/$(PROJECT).bin
	@echo CREATE $@
	$(PYTHON) $(TOP)/tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY_ID) -b 0x0 -c -o $@ $^
