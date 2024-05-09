SRC_S += $(SRC_S_GCC)

# Assembly files can be name with upper case .S, convert it to .s
SRC_S := $(SRC_S:.S=.s)

# Due to GCC LTO bug https://bugs.launchpad.net/gcc-arm-embedded/+bug/1747966
# assembly file should be placed first in linking order
# '_asm' suffix is added to object of assembly file
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_S:.s=_asm.o))
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_C:.c=.o))

CFLAGS += $(CFLAGS_GCC) -MD

# LTO makes it difficult to analyze map file for optimizing size purpose
# We will run this option in ci
ifeq ($(NO_LTO),1)
CFLAGS := $(filter-out -flto,$(CFLAGS))
endif

ifneq ($(CFLAGS_SKIP),)
CFLAGS := $(filter-out $(CFLAGS_SKIP),$(CFLAGS))
endif

ifeq ($(TOOLCHAIN),clang)
CFLAGS += $(CFLAGS_CLANG)
LDFLAGS += $(CFLAGS) $(LDFLAGS_CLANG)
else
LDFLAGS += $(CFLAGS) $(LDFLAGS_GCC)
endif

# TODO should be removed after all examples are updated
ifdef LD_FILE
LDFLAGS += -Wl,-T,$(TOP)/$(LD_FILE)
endif

ifdef LD_FILE_GCC
LDFLAGS += -Wl,-T,$(TOP)/$(LD_FILE_GCC)
endif

ASFLAGS += $(CFLAGS)

# libc
ifneq ($(BOARD), spresense)
LIBS += -lc
endif

# ---------------------------------------
# Rules
# ---------------------------------------

# Compile .c file
$(BUILD)/obj/%.o: %.c
	@echo CC $(notdir $@)
	@$(CC) $(CFLAGS) -c -o $@ $<

# ASM sources lower case .s
$(BUILD)/obj/%_asm.o: %.s
	@echo AS $(notdir $@)
	@$(AS) $(ASFLAGS) -c -o $@ $<

# ASM sources upper case .S
$(BUILD)/obj/%_asm.o: %.S
	@echo AS $(notdir $@)
	@$(AS) $(ASFLAGS) -c -o $@ $<

OBJCOPY_BIN_OPTION ?=
$(BUILD)/$(PROJECT).bin: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	$(OBJCOPY) -O binary $(OBJCOPY_BIN_OPTION) $^ $@

$(BUILD)/$(PROJECT).hex: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	@$(OBJCOPY) -O ihex $^ $@

$(BUILD)/$(PROJECT).elf: $(OBJ)
	@echo LINK $@
	@$(LD) -o $@ $(LDFLAGS) $^ -Wl,--start-group $(LIBS) -Wl,--end-group
