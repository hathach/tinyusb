# ---------------------------------------
# Common make rules for all examples
# ---------------------------------------

# Set all as default goal
.DEFAULT_GOAL := all

# ---------------- GNU Make Start -----------------------
# ESP32-Sx and RP2040 has its own CMake build system
ifeq (,$(findstring $(FAMILY),espressif rp2040))

# ---------------------------------------
# Compiler Flags
# ---------------------------------------

CFLAGS += $(addprefix -I,$(INC))

# Verbose mode
ifeq ("$(V)","1")
$(info CFLAGS  $(CFLAGS) ) $(info )
$(info LDFLAGS $(LDFLAGS)) $(info )
$(info ASFLAGS $(ASFLAGS)) $(info )
endif

# ---------------------------------------
# Rules
# ---------------------------------------

all: $(BUILD)/$(PROJECT).bin $(BUILD)/$(PROJECT).hex size

uf2: $(BUILD)/$(PROJECT).uf2

# We set vpath to point to the top of the tree so that the source files
# can be located. By following this scheme, it allows a single build rule
# to be used to compile all .c files.
vpath %.c . $(TOP)
vpath %.s . $(TOP)
vpath %.S . $(TOP)

include $(TOP)/tools/make/toolchain/arm_$(TOOLCHAIN)_rules.mk


OBJ_DIRS = $(sort $(dir $(OBJ)))
$(OBJ): | $(OBJ_DIRS)
$(OBJ_DIRS):
ifeq ($(CMDEXE),1)
	-@$(MKDIR) $(subst /,\,$@)
else
	@$(MKDIR) -p $@
endif

# UF2 generation, iMXRT need to strip to text only before conversion
ifneq ($(FAMILY),imxrt)
$(BUILD)/$(PROJECT).uf2: $(BUILD)/$(PROJECT).hex
	@echo CREATE $@
	$(PYTHON) $(TOP)/tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY_ID) -c -o $@ $^
endif

copy-artifact: $(BUILD)/$(PROJECT).bin $(BUILD)/$(PROJECT).hex $(BUILD)/$(PROJECT).uf2

endif
# ---------------- GNU Make End -----------------------

.PHONY: clean
clean:
ifeq ($(CMDEXE),1)
	rd /S /Q $(subst /,\,$(BUILD))
else
	$(RM) -rf $(BUILD)
endif

# get depenecies
.PHONY: get-deps
get-deps:
	$(PYTHON) $(TOP)/tools/get_deps.py $(DEPS_SUBMODULES)

.PHONY: size
size: $(BUILD)/$(PROJECT).elf
	-@echo ''
	@$(SIZE) $<
	-@echo ''

# linkermap must be install previously at https://github.com/hathach/linkermap
linkermap: $(BUILD)/$(PROJECT).elf
	@linkermap -v $<.map

# ---------------------------------------
# Flash Targets
# ---------------------------------------

# Jlink binary
ifeq ($(OS),Windows_NT)
  JLINKEXE = JLink.exe
else
  JLINKEXE = JLinkExe
endif

# Jlink Interface
JLINK_IF ?= swd

# Jlink script
define jlink_script
halt
loadfile $^
r
go
exit
endef
export jlink_script

# Flash using jlink
flash-jlink: $(BUILD)/$(PROJECT).hex
	@echo "$$jlink_script" > $(BUILD)/$(BOARD).jlink
	$(JLINKEXE) -device $(JLINK_DEVICE) -if $(JLINK_IF) -JTAGConf -1,-1 -speed auto -CommandFile $(BUILD)/$(BOARD).jlink

# Flash STM32 MCU using stlink with STM32 Cube Programmer CLI
flash-stlink: $(BUILD)/$(PROJECT).elf
	STM32_Programmer_CLI --connect port=swd --write $< --go

$(BUILD)/$(PROJECT)-sunxi.bin: $(BUILD)/$(PROJECT).bin
	$(PYTHON) $(TOP)/tools/mksunxi.py $< $@

flash-xfel: $(BUILD)/$(PROJECT)-sunxi.bin
	xfel spinor write 0 $<
	xfel reset

# Flash using pyocd
PYOCD_OPTION ?=
flash-pyocd: $(BUILD)/$(PROJECT).hex
	pyocd flash -t $(PYOCD_TARGET) $(PYOCD_OPTION) $<
	#pyocd reset -t $(PYOCD_TARGET)

# Flash using openocd
OPENOCD_OPTION ?=
flash-openocd: $(BUILD)/$(PROJECT).elf
	openocd $(OPENOCD_OPTION) -c "program $< verify reset exit"

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
ifeq ($(CMDEXE),1)
	@$(MKDIR) $(subst /,\,$@)
else
	@$(MKDIR) -p $@
endif

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
