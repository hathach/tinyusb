# ---------------------------------------
# Common make rules for all examples
# ---------------------------------------

# Set all as default goal
.DEFAULT_GOAL := all

# ESP32-SX and RP2040 has its own CMake build system
ifneq ($(FAMILY),esp32s2)
ifneq ($(FAMILY),esp32s3)
ifneq ($(FAMILY),rp2040)
# ---------------------------------------
# GNU Make build system
# ---------------------------------------

# libc
LIBS += -lgcc -lm -lnosys

ifneq ($(BOARD), spresense)
LIBS += -lc
endif

# TinyUSB Stack source
SRC_C += \
	src/tusb.c \
	src/common/tusb_fifo.c \
	src/device/usbd.c \
	src/device/usbd_control.c \
	src/class/audio/audio_device.c \
	src/class/cdc/cdc_device.c \
	src/class/dfu/dfu_device.c \
	src/class/dfu/dfu_rt_device.c \
	src/class/hid/hid_device.c \
	src/class/midi/midi_device.c \
	src/class/msc/msc_device.c \
	src/class/net/net_device.c \
	src/class/usbtmc/usbtmc_device.c \
	src/class/vendor/vendor_device.c

# TinyUSB stack include
INC += $(TOP)/src

CFLAGS += $(addprefix -I,$(INC))

LDFLAGS += $(CFLAGS) -Wl,-T,$(TOP)/$(LD_FILE) -Wl,-Map=$@.map -Wl,-cref -Wl,-gc-sections
ifneq ($(SKIP_NANOLIB), 1)
LDFLAGS += -specs=nosys.specs -specs=nano.specs
endif

ASFLAGS += $(CFLAGS)

# Assembly files can be name with upper case .S, convert it to .s
SRC_S := $(SRC_S:.S=.s)

# Due to GCC LTO bug https://bugs.launchpad.net/gcc-arm-embedded/+bug/1747966
# assembly file should be placed first in linking order
# '_asm' suffix is added to object of assembly file
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_S:.s=_asm.o))
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_C:.c=.o))

# Verbose mode
ifeq ("$(V)","1")
$(info CFLAGS  $(CFLAGS) ) $(info )
$(info LDFLAGS $(LDFLAGS)) $(info )
$(info ASFLAGS $(ASFLAGS)) $(info )
endif

all: $(BUILD)/$(PROJECT).bin $(BUILD)/$(PROJECT).hex size

uf2: $(BUILD)/$(PROJECT).uf2

OBJ_DIRS = $(sort $(dir $(OBJ)))
$(OBJ): | $(OBJ_DIRS)
$(OBJ_DIRS):
ifeq ($(CMDEXE),1)
	@$(MKDIR) $(subst /,\,$@)
else
	@$(MKDIR) -p $@
endif

$(BUILD)/$(PROJECT).elf: $(OBJ)
	@echo LINK $@
	@$(CC) -o $@ $(LDFLAGS) $^ -Wl,--start-group $(LIBS) -Wl,--end-group

$(BUILD)/$(PROJECT).bin: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	@$(OBJCOPY) -O binary $^ $@

$(BUILD)/$(PROJECT).hex: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	@$(OBJCOPY) -O ihex $^ $@

# UF2 generation, iMXRT need to strip to text only before conversion
ifeq ($(FAMILY),imxrt)
$(BUILD)/$(PROJECT).uf2: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	@$(OBJCOPY) -O ihex -R .flash_config -R .ivt $^ $(BUILD)/$(PROJECT)-textonly.hex
	$(PYTHON) $(TOP)/tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY_ID) -c -o $@ $(BUILD)/$(PROJECT)-textonly.hex
else
$(BUILD)/$(PROJECT).uf2: $(BUILD)/$(PROJECT).hex
	@echo CREATE $@
	$(PYTHON) $(TOP)/tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY_ID) -c -o $@ $^
endif

copy-artifact: $(BUILD)/$(PROJECT).bin $(BUILD)/$(PROJECT).hex $(BUILD)/$(PROJECT).uf2

# We set vpath to point to the top of the tree so that the source files
# can be located. By following this scheme, it allows a single build rule
# to be used to compile all .c files.
vpath %.c . $(TOP)
$(BUILD)/obj/%.o: %.c
	@echo CC $(notdir $@)
	@$(CC) $(CFLAGS) -c -MD -o $@ $<

# ASM sources lower case .s
vpath %.s . $(TOP)
$(BUILD)/obj/%_asm.o: %.s
	@echo AS $(notdir $@)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

# ASM sources upper case .S
vpath %.S . $(TOP)
$(BUILD)/obj/%_asm.o: %.S
	@echo AS $(notdir $@)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

size: $(BUILD)/$(PROJECT).elf
	-@echo ''
	@$(SIZE) $<
	-@echo ''

.PHONY: clean
clean:
ifeq ($(CMDEXE),1)
	rd /S /Q $(subst /,\,$(BUILD))
else
	$(RM) -rf $(BUILD)
endif

endif
endif
endif # GNU Make

# ---------------------------------------
# Flash Targets
# ---------------------------------------

# Flash binary using Jlink
ifeq ($(OS),Windows_NT)
  JLINKEXE = JLink.exe
else
  JLINKEXE = JLinkExe
endif

JLINK_IF ?= swd

# Flash using jlink
flash-jlink: $(BUILD)/$(PROJECT).hex
	@echo halt > $(BUILD)/$(BOARD).jlink
	@echo r > $(BUILD)/$(BOARD).jlink
	@echo loadfile $^ >> $(BUILD)/$(BOARD).jlink
	@echo r >> $(BUILD)/$(BOARD).jlink
	@echo go >> $(BUILD)/$(BOARD).jlink
	@echo exit >> $(BUILD)/$(BOARD).jlink
	$(JLINKEXE) -device $(JLINK_DEVICE) -if $(JLINK_IF) -JTAGConf -1,-1 -speed auto -CommandFile $(BUILD)/$(BOARD).jlink

# flash STM32 MCU using stlink with STM32 Cube Programmer CLI
flash-stlink: $(BUILD)/$(PROJECT).elf
	STM32_Programmer_CLI --connect port=swd --write $< --go

# flash with pyocd
flash-pyocd: $(BUILD)/$(PROJECT).hex
	pyocd flash -t $(PYOCD_TARGET) $<
	pyocd reset -t $(PYOCD_TARGET)

# flash with Black Magic Probe

# This symlink is created by https://github.com/blacksphere/blackmagic/blob/master/driver/99-blackmagic.rules
BMP ?= /dev/ttyBmpGdb

flash-bmp: $(BUILD)/$(PROJECT).elf
	$(GDB) --batch -ex 'target extended-remote $(BMP)' -ex 'monitor swdp_scan' -ex 'attach 1' -ex load  $<

debug-bmp: $(BUILD)/$(PROJECT).elf
	$(GDB) -ex 'target extended-remote $(BMP)' -ex 'monitor swdp_scan' -ex 'attach 1' $<

#-------------- Artifacts --------------

# Create binary directory
$(BIN):
	@$(MKDIR) -p $@

# Copy binaries .elf, .bin, .hex, .uf2 to BIN for upload
# due to large size of combined artifacts, only uf2 is uploaded for now
copy-artifact: $(BIN)
	@$(CP) $(BUILD)/$(PROJECT).uf2 $(BIN)
	#@$(CP) $(BUILD)/$(PROJECT).bin $(BIN)
	#@$(CP) $(BUILD)/$(PROJECT).hex $(BIN)
	#@$(CP) $(BUILD)/$(PROJECT).elf $(BIN)

# Print out the value of a make variable.
# https://stackoverflow.com/questions/16467718/how-to-print-out-a-variable-in-makefile
print-%:
	@echo $* = $($*)
