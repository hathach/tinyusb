# ---------------------------------------
# Common make rules for all examples
# ---------------------------------------

# Set all as default goal
.DEFAULT_GOAL := all

# ---------------------------------------
# Compiler Flags
# ---------------------------------------

LIBS_GCC ?=  -lm

# libc
LIBS += $(LIBS_GCC)

ifneq ($(BOARD), spresense)
LIBS += -lc -Wl,-Bstatic -lc++ -Wl,-Bdynamic
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
	src/class/net/ecm_rndis_device.c \
	src/class/net/ncm_device.c \
	src/class/usbtmc/usbtmc_device.c \
	src/class/video/video_device.c \
	src/class/vendor/vendor_device.c


# Fuzzers are c++
SRC_CXX += \
	test/fuzz/dcd_fuzz.cc \
	test/fuzz/fuzz.cc \
	test/fuzz/msc_fuzz.cc \
	test/fuzz/net_fuzz.cc \
	test/fuzz/usbd_fuzz.cc

# TinyUSB stack include
INC += $(TOP)/src

CFLAGS += $(addprefix -I,$(INC))
CXXFLAGS += -std=c++17

# LTO makes it difficult to analyze map file for optimizing size purpose
# We will run this option in ci
ifeq ($(NO_LTO),1)
CFLAGS := $(filter-out -flto,$(CFLAGS))
endif

ifneq ($(LD_FILE),)
LDFLAGS_LD_FILE ?= -Wl,-T,$(TOP)/$(LD_FILE)
endif

LDFLAGS += $(CFLAGS) $(LDFLAGS_LD_FILE) -fuse-ld=lld -Wl,-Map=$@.map -Wl,--cref -Wl,-gc-sections
ifneq ($(SKIP_NANOLIB), 1)
endif

ASFLAGS += $(CFLAGS)

# Assembly files can be name with upper case .S, convert it to .s
SRC_S := $(SRC_S:.S=.s)

# Due to GCC LTO bug https://bugs.launchpad.net/gcc-arm-embedded/+bug/1747966
# assembly file should be placed first in linking order
# '_asm' suffix is added to object of assembly file
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_S:.s=_asm.o))
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_CXX:.cc=_cxx.o))

# Verbose mode
ifeq ("$(V)","1")
$(info CFLAGS  $(CFLAGS) ) $(info )
$(info LDFLAGS $(LDFLAGS)) $(info )
$(info ASFLAGS $(ASFLAGS)) $(info )
endif

# ---------------------------------------
# Rules
# ---------------------------------------

all: $(BUILD)/$(PROJECT) 

OBJ_DIRS = $(sort $(dir $(OBJ)))
$(OBJ): | $(OBJ_DIRS)
$(OBJ_DIRS):
ifeq ($(CMDEXE),1)
	@$(MKDIR) $(subst /,\,$@)
else
	@$(MKDIR) -p $@
endif

$(BUILD)/$(PROJECT): $(OBJ)
	@echo LINK $@
	@ $(CXX) -o $@  $(LIB_FUZZING_ENGINE) $^ $(LIBS) $(LDFLAGS)

# We set vpath to point to the top of the tree so that the source files
# can be located. By following this scheme, it allows a single build rule
# to be used to compile all .c files.
vpath %.c . $(TOP)
$(BUILD)/obj/%.o: %.c
	@echo CC $(notdir $@)
	@$(CC) $(CFLAGS) -c -MD -o $@ $<
	
# All cpp srcs
vpath %.cc . $(TOP)
$(BUILD)/obj/%_cxx.o: %.cc
	@echo CXX $(notdir $@)
	@$(CXX) $(CFLAGS) $(CXXFLAGS) -c -MD -o $@ $<

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

.PHONY: clean
clean:
ifeq ($(CMDEXE),1)
	rd /S /Q $(subst /,\,$(BUILD))
else
	$(RM) -rf $(BUILD)
endif
# ---------------- GNU Make End -----------------------

# get depenecies
.PHONY: get-deps
get-deps:
  ifdef DEPS_SUBMODULES
	git -C $(TOP) submodule update --init $(DEPS_SUBMODULES)
  endif

size: $(BUILD)/$(PROJECT)
	-@echo ''
	@$(SIZE) $<
	-@echo ''

# linkermap must be install previously at https://github.com/hathach/linkermap
linkermap: $(BUILD)/$(PROJECT)
	@linkermap -v $<.map

# Print out the value of a make variable.
# https://stackoverflow.com/questions/16467718/how-to-print-out-a-variable-in-makefile
print-%:
	@echo $* = $($*)