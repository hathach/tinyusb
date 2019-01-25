include ../../../tools/top.mk

# Select the board to build for.
ifeq ($(BOARD),)
  $(error You must provide a BOARD parameter)
else
  ifeq ($(wildcard $(TOP)/hw/bsp/$(BOARD)/.),)
    $(error Invalid BOARD specified)
  endif
endif

# If the build directory is not given, make it reflect the board name.
BUILD ?= build-$(BOARD)

CROSS_COMPILE = arm-none-eabi-

include $(TOP)/hw/bsp/$(BOARD)/board.mk

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
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
	-Wcast-align \
	-Wno-error=lto-type-mismatch \
	-ffunction-sections \
	-fdata-sections

#Debugging/Optimization
ifeq ($(DEBUG), 1)
  CFLAGS += -O0 -ggdb
else
  CFLAGS += -flto -Os
endif

CFLAGS += $(INC) -Wall -Werror -std=gnu11 -nostdlib -DBOARD_$(shell echo $(BOARD) | tr a-z A-Z)

LDFLAGS += $(CFLAGS) -nostartfiles -fshort-enums -Wl,-T,$(TOP)/$(LD_FILE) -Wl,-Map=$@.map -Wl,-cref -Wl,-gc-sections -specs=nosys.specs -specs=nano.specs
LIBS := -lgcc -lc -lm -lnosys

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

OBJ += $(addprefix $(BUILD)/obj/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_S:.S=.o))

all: $(BUILD)/$(BOARD)-firmware.bin

OBJ_DIRS = $(sort $(dir $(OBJ)))
$(OBJ): | $(OBJ_DIRS)
$(OBJ_DIRS):
	@$(MKDIR) -p $@

$(BUILD)/$(BOARD)-firmware.elf: $(OBJ)
	@echo LINK $@
	@$(CC) -o $@ $(LDFLAGS) $^ -Wl,--start-group $(LIBS) -Wl,--end-group

$(BUILD)/$(BOARD)-firmware.bin: $(BUILD)/$(BOARD)-firmware.elf
	@echo CREATE $@
	$(OBJCOPY) -O binary -j .vectors -j .text -j .data $^ $@

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

# ASM sources
vpath %.S . $(TOP)
$(BUILD)/obj/%.o: %.S
	@echo AS $(notdir $@)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

clean:
	rm -rf build-$(BOARD)
