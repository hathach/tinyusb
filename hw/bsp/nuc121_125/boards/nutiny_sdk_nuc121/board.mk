NUC_SERIES = nuc121
JLINK_DEVICE = NUC121SC2AE
LD_FILE = $(BOARD_PATH)/nuc121_flash.ld

# Extra StdDriver sources for NUC121
SRC_C += \
  hw/mcu/nuvoton/nuc121_125/StdDriver/src/fmc.c \
  hw/mcu/nuvoton/nuc121_125/StdDriver/src/sys.c \
  hw/mcu/nuvoton/nuc121_125/StdDriver/src/timer.c

# Flash using Nuvoton's openocd fork at https://github.com/OpenNuvoton/OpenOCD-Nuvoton
# Please compile and install it from github source
flash: $(BUILD)/$(PROJECT).elf
	openocd -f interface/nulink.cfg -f target/numicroM0.cfg -c "program $< reset exit"
