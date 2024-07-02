LD_FILE = $(BOARD_PATH)/max32650.ld

# For flash-jlink target
JLINK_DEVICE = max32650

# flash target using Jlik
flash: flash-jlink

# Optional flash option when running within an installed MSDK to use OpenOCD
# Mainline OpenOCD does not yet have the MAX32's flash algorithm integrated.
# If the MSDK is installed, flash-msdk can be run to utilize the the modified
# openocd with the algorithms
MAXIM_PATH := $(subst \,/,$(MAXIM_PATH))
flash-msdk: $(BUILD)/$(PROJECT).elf
	$(MAXIM_PATH)/Tools/OpenOCD/openocd -s $(MAXIM_PATH)/Tools/OpenOCD/scripts \
		-f interface/cmsis-dap.cfg -f target/max32650.cfg \
		-c "program $(BUILD)/$(PROJECT).elf verify; init; reset; exit"
