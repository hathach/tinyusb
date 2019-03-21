include ../../../tools/top.mk

# Select the board to build for.
ifeq ($(BOARD),)
  $(info You must provide a BOARD parameter with 'BOARD=')
  $(info Possible values are:)
  $(info $(sort $(subst /.,,$(subst $(TOP)/hw/bsp/,,$(wildcard $(TOP)/hw/bsp/*/.)))))
  $(error BOARD not defined)
else
  ifeq ($(wildcard $(TOP)/hw/bsp/$(BOARD)/.),)
    $(error Invalid BOARD specified)
  endif
endif

# Verbose mode (V=). 0: default, 1: print out CFLAG, LDFLAG 2: print all compile command
ifeq ("$(V)","2")
  QUIET =
else
  QUIET = @
endif

# If the build directory is not given, make it reflect the board name.
BUILD ?= build-$(BOARD)

CROSS_COMPILE = arm-none-eabi-

include $(TOP)/hw/bsp/$(BOARD)/board.mk

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size
MKDIR = mkdir
SED = sed
CP = cp
RM = rm

INC += -Isrc \
	-I$(TOP)/hw \
	-I$(TOP)/src

CFLAGS += \
	-fsingle-precision-constant \
	-fno-strict-aliasing \
	-Wdouble-promotion \
	-Wno-endif-labels \
	-Wstrict-prototypes \
	-Werror-implicit-function-declaration \
	-Wfloat-equal \
	-Wundef \
	-Wshadow \
	-Wwrite-strings \
	-Wsign-compare \
	-Wmissing-format-attribute \
	-Wno-deprecated-declarations \
	-Wnested-externs \
	-Wunreachable-code \
	-Wno-error=lto-type-mismatch \
	-ffunction-sections \
	-fdata-sections

# This causes lots of warning with nrf5x build due to nrfx code
# CFLAGS += -Wcast-align

#Debugging/Optimization
ifeq ($(DEBUG), 1)
  CFLAGS += -O0 -ggdb
else
  CFLAGS += -flto -Os
endif

CFLAGS += $(INC) -Wall -Werror -std=gnu11 -DBOARD_$(shell echo $(BOARD) | tr a-z\- A-Z_)
LDFLAGS += $(CFLAGS) -fshort-enums -Wl,-T,$(TOP)/$(LD_FILE) -Wl,-Map=$@.map -Wl,-cref -Wl,-gc-sections -specs=nosys.specs -specs=nano.specs

ifeq ("$(V)","1")
$(info CFLAGS  $(CFLAGS))
$(info )
$(info LDFLAGS $(LDFLAGS))
$(info )
$(info ASFLAGS $(ASFLAGS))
$(info )
endif

LIBS = -lgcc -lc -lm -lnosys

EXAMPLE_SOURCE += \
	src/main.c \
	src/msc_app.c \
	src/msc_disk_ram.c \
	src/tusb_descriptors.c

SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

LIB_SOURCE += \
	hw/bsp/$(BOARD)/board_$(BOARD).c \
	src/common/tusb_fifo.c \
	src/device/usbd.c \
	src/device/usbd_auto_desc.c \
	src/device/usbd_control.c \
	src/class/msc/msc_device.c \
	src/class/cdc/cdc_device.c \
	src/class/hid/hid_device.c \
	src/tusb.c \
	src/portable/$(VENDOR)/$(CHIP_FAMILY)/dcd_$(CHIP_FAMILY).c

SRC_C += $(LIB_SOURCE)

# Assembly files can be name with upper case .S, convert it to .s 
SRC_S := $(SRC_S:.S=.s)

# Due to GCC LTO bug https://bugs.launchpad.net/gcc-arm-embedded/+bug/1747966
# assembly file should be placed first in linking order
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_S:.s=.o))
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_C:.c=.o))

# Set all as default goal
.DEFAULT_GOAL := all
all: $(BUILD)/$(BOARD)-firmware.bin size

OBJ_DIRS = $(sort $(dir $(OBJ)))
$(OBJ): | $(OBJ_DIRS)
$(OBJ_DIRS):
	@$(MKDIR) -p $@

$(BUILD)/$(BOARD)-firmware.elf: $(OBJ)
	@echo LINK $@
	$(QUIET)$(CC) -o $@ $(LDFLAGS) $^ -Wl,--start-group $(LIBS) -Wl,--end-group

$(BUILD)/$(BOARD)-firmware.bin: $(BUILD)/$(BOARD)-firmware.elf
	@echo CREATE $@
	@$(OBJCOPY) -O binary -j .vectors -j .text -j .data $^ $@
	
$(BUILD)/$(BOARD)-firmware.hex: $(BUILD)/$(BOARD)-firmware.elf	
	@echo CREATE $@
	@$(OBJCOPY) -O ihex $^ $@

# We set vpath to point to the top of the tree so that the source files
# can be located. By following this scheme, it allows a single build rule
# to be used to compile all .c files.
vpath %.c . $(TOP)
$(BUILD)/obj/%.o: %.c
	@echo CC $(notdir $@)
	$(QUIET)$(CC) $(CFLAGS) -c -MD -o $@ $<
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
	$(QUIET)$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

# ASM sources upper case .S
vpath %.S . $(TOP)
$(BUILD)/obj/%.o: %.S
	@echo AS $(notdir $@)
	$(QUIET)$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

# Flash binary using Jlink
ifeq ($(OS),Windows_NT)
  JLINKEXE = JLink.exe
else 
  JLINKEXE = JLinkExe
endif

# default jlink interface is swd
ifeq ($(JLINK_IF),)
  JLINK_IF = swd
endif

# Flash using jlink
flash-jlink: $(BUILD)/$(BOARD)-firmware.hex
	@echo halt > $(BUILD)/$(BOARD).jlink
	@echo loadfile $^ >> $(BUILD)/$(BOARD).jlink
	@echo r >> $(BUILD)/$(BOARD).jlink
	@echo go >> $(BUILD)/$(BOARD).jlink
	@echo exit >> $(BUILD)/$(BOARD).jlink
	$(JLINKEXE) -device $(JLINK_DEVICE) -if $(JLINK_IF) -speed auto -CommandFile $(BUILD)/$(BOARD).jlink

size: $(BUILD)/$(BOARD)-firmware.elf
	-@echo ''
	@$(SIZE) $<
	-@echo ''

clean:
	rm -rf build-$(BOARD)
