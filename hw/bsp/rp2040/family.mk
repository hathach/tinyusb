JLINK_DEVICE = rp2040_m0_0
PYOCD_TARGET = rp2040

ifeq ($(DEBUG), 1)
CMAKE_DEFSYM += -DCMAKE_BUILD_TYPE=Debug
endif

$(BUILD):
	cmake -S . -B $(BUILD) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) -DPICO_BUILD_DOCS=0 $(CMAKE_DEFSYM)

all: $(BUILD)
	$(MAKE) -C $(BUILD)

clean:
	$(RM) -rf $(BUILD)

flash: flash-pyocd
flash-uf2:
	@$(CP) $(BUILD)/$(PROJECT).uf2 /media/$(USER)/RPI-RP2
