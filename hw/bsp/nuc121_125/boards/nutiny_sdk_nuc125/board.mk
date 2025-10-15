NUC_SERIES = nuc125
JLINK_DEVICE = NUC125SC2AE
LD_FILE = $(BOARD_PATH)/nuc125_flash.ld

# Flash using Nuvoton's openocd fork at https://github.com/OpenNuvoton/OpenOCD-Nuvoton
# Please compile and install it from github source
flash: $(BUILD)/$(PROJECT).elf
	openocd -f interface/nulink.cfg -f target/numicroM0.cfg -c "program $< reset exit"
