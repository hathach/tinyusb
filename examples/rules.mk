#
# Common make definition for all examples
#

# libc
LIBS = -lgcc -lc -lm -lnosys

# TinyUSB Stack source
SRC_C += \
	src/common/tusb_fifo.c \
	src/device/usbd.c \
	src/device/usbd_control.c \
	src/class/msc/msc_device.c \
	src/class/cdc/cdc_device.c \
	src/class/hid/hid_device.c \
	src/class/midi/midi_device.c \
	src/tusb.c \
	src/portable/$(VENDOR)/$(CHIP_FAMILY)/dcd_$(CHIP_FAMILY).c

# TinyUSB stack include
INC += $(TOP)/src

#
CFLAGS += $(addprefix -I,$(INC))
LDFLAGS += $(CFLAGS) -fshort-enums -Wl,-T,$(TOP)/$(LD_FILE) -Wl,-Map=$@.map -Wl,-cref -Wl,-gc-sections -specs=nosys.specs -specs=nano.specs
ASFLAGS += $(CFLAGS)

# Assembly files can be name with upper case .S, convert it to .s 
SRC_S := $(SRC_S:.S=.s)

# Due to GCC LTO bug https://bugs.launchpad.net/gcc-arm-embedded/+bug/1747966
# assembly file should be placed first in linking order
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_S:.s=.o))
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_C:.c=.o))

# Verbose mode
ifeq ("$(V)","1")
$(info CFLAGS  $(CFLAGS) ) $(info )
$(info LDFLAGS $(LDFLAGS)) $(info )
$(info ASFLAGS $(ASFLAGS)) $(info )
endif

# Set all as default goal
.DEFAULT_GOAL := all
all: $(BUILD)/$(BOARD)-firmware.bin size

uf2: $(BUILD)/$(BOARD)-firmware.uf2

OBJ_DIRS = $(sort $(dir $(OBJ)))
$(OBJ): | $(OBJ_DIRS)
$(OBJ_DIRS):
	@$(MKDIR) -p $@

$(BUILD)/$(BOARD)-firmware.elf: $(OBJ)
	@echo LINK $@
	@$(CC) -o $@ $(LDFLAGS) $^ -Wl,--start-group $(LIBS) -Wl,--end-group

$(BUILD)/$(BOARD)-firmware.bin: $(BUILD)/$(BOARD)-firmware.elf
	@echo CREATE $@
	@$(OBJCOPY) -O binary $^ $@
	
$(BUILD)/$(BOARD)-firmware.hex: $(BUILD)/$(BOARD)-firmware.elf	
	@echo CREATE $@
	@$(OBJCOPY) -O ihex $^ $@

UF2_FAMILY ?= 0x00
$(BUILD)/$(BOARD)-firmware.uf2: $(BUILD)/$(BOARD)-firmware.hex
	@echo CREATE $@
	$(PYTHON) $(TOP)/tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY) -c -o $@ $^

# We set vpath to point to the top of the tree so that the source files
# can be located. By following this scheme, it allows a single build rule
# to be used to compile all .c files.
vpath %.c . $(TOP)
$(BUILD)/obj/%.o: %.c
	@echo CC $(notdir $@)
	@$(CC) $(CFLAGS) -c -MD -o $@ $<
	@# The following fixes the dependency file.
	@# See http://make.paulandlesley.org/autodep.html for details.
	@# Regex adjusted from the above to play better with Windows paths, etc.
	@$(CP) $(@:.o=.d) $(@:.o=.P); \
	  $(SED) -e 's/#.*//' -e 's/^.*:  *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:.o=.d) >> $(@:.o=.P); \
	  $(RM) $(@:.o=.d)

# ASM sources lower case .s
vpath %.s . $(TOP)
$(BUILD)/obj/%.o: %.s
	@echo AS $(notdir $@)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

# ASM sources upper case .S
vpath %.S . $(TOP)
$(BUILD)/obj/%.o: %.S
	@echo AS $(notdir $@)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

size: $(BUILD)/$(BOARD)-firmware.elf
	-@echo ''
	@$(SIZE) $<
	-@echo ''

clean:
	rm -rf $(BUILD)
	
# Flash binary using Jlink
ifeq ($(OS),Windows_NT)
  JLINKEXE = JLink.exe
else 
  JLINKEXE = JLinkExe
endif

# Flash using jlink
flash-jlink: $(BUILD)/$(BOARD)-firmware.hex
	@echo halt > $(BUILD)/$(BOARD).jlink
	@echo loadfile $^ >> $(BUILD)/$(BOARD).jlink
	@echo r >> $(BUILD)/$(BOARD).jlink
	@echo go >> $(BUILD)/$(BOARD).jlink
	@echo exit >> $(BUILD)/$(BOARD).jlink
	$(JLINKEXE) -device $(JLINK_DEVICE) -if $(JLINK_IF) -JTAGConf -1,-1 -speed auto -CommandFile $(BUILD)/$(BOARD).jlink
