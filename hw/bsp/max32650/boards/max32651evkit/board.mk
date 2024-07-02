LD_FILE = $(BOARD_PATH)/max32651.ld
CFLAGS += -D__SLA_FWK__

# For flash-jlink target
JLINK_DEVICE = max32650

# flash target using MSDK signing the image
flash: flash-msdk-signed


MAXIM_PATH := $(subst \,/,$(MAXIM_PATH))

# The MAX32651EVKIT is pin for pin identical to the MAX32650EVKIT, however the
# MAX32651 has a secure bootloader which requires the image to be signed before
# loading into flash. All MAX32651EVKIT's have the same key for evaluation
# purposes, so create a special flash rule to sign the binary and flash using
# the MSDK.
# For the MAX32650, the regular flash, flash-jlink and flash-msdk are sufficient
MCU_PATH = $(TOP)/hw/mcu/analog/max32/
# Assume no extension for sign utility
SIGN_EXE = sign_app
ifeq ($(OS), Windows_NT)
# Must use .exe extension on Windows, since the binaries
# for Linux may live in the same place.
SIGN_EXE := sign_app.exe
else
UNAME = $(shell uname -s)
ifneq ($(findstring MSYS_NT,$(UNAME)),)
# Must also use .exe extension for MSYS2
SIGN_EXE := sign_app.exe
endif
endif

flash-msdk-signed: $(BUILD)/$(PROJECT).elf
	$(OBJCOPY) $(BUILD)/$(PROJECT).elf -R .sig -O binary $(BUILD)/$(PROJECT).bin
	$(MCU_PATH)/Tools/SBT/bin/$(SIGN_EXE) -c MAX32651 key_file="$(MCU_PATH)/Tools/SBT/devices/MAX32651/keys/maximtestcrk.key" \
		ca=$(BUILD)/$(PROJECT).bin sca=$(BUILD)/$(PROJECT).sbin
	$(OBJCOPY) $(BUILD)/$(PROJECT).elf --update-section .sig=$(BUILD)/$(PROJECT).sig
	$(MAXIM_PATH)/Tools/OpenOCD/openocd -s $(MAXIM_PATH)/Tools/OpenOCD/scripts \
		-f interface/cmsis-dap.cfg -f target/max32650.cfg \
		-c "program $(BUILD)/$(PROJECT).elf verify; init; reset; exit"

