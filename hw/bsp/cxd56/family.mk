include $(TOP)/$(BOARD_PATH)/board.mk

# Platforms are: Linux, Darwin, MSYS, CYGWIN
PLATFORM := $(firstword $(subst _, ,$(shell uname -s 2>/dev/null)))

ifeq ($(PLATFORM),Darwin)
  # macOS
  MKSPK = $(TOP)/hw/mcu/sony/cxd56/mkspk/mkspk
else ifeq ($(PLATFORM),Linux)
  # Linux
  MKSPK = $(TOP)/hw/mcu/sony/cxd56/mkspk/mkspk
else
  # Cygwin/MSYS2
  MKSPK = $(TOP)/hw/mcu/sony/cxd56/mkspk/mkspk.exe
endif

CFLAGS += \
	-DCONFIG_HAVE_DOUBLE \
	-Dmain=spresense_main \
	-pipe \
	-std=gnu11 \
	-fno-strength-reduce \
	-fomit-frame-pointer \
	-Wno-error=undef \
	-Wno-error=cast-align \
	-Wno-error=unused-parameter \
	-DCFG_TUSB_MCU=OPT_MCU_CXD56 \

CPU_CORE ?= cortex-m4

# suppress following warnings from mcu driver
# lwip/src/core/raw.c:334:43: error: declaration of 'recv' shadows a global declaration
CFLAGS += -Wno-error=shadow  -Wno-error=redundant-decls

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

SPRESENSE_SDK = $(TOP)/hw/mcu/sony/cxd56/spresense-exported-sdk

SRC_C += src/portable/sony/cxd56/dcd_cxd56.c

INC += \
	$(SPRESENSE_SDK)/nuttx/include \
	$(SPRESENSE_SDK)/nuttx/arch \
	$(SPRESENSE_SDK)/nuttx/arch/chip \
	$(SPRESENSE_SDK)/nuttx/arch/os \
	$(SPRESENSE_SDK)/sdk/include \
	$(TOP)/$(BOARD_PATH)

LIBS += \
	$(SPRESENSE_SDK)/nuttx/libs/libapps.a \
	$(SPRESENSE_SDK)/nuttx/libs/libnuttx.a \

LD_FILE = hw/mcu/sony/cxd56/spresense-exported-sdk/nuttx/scripts/ramconfig.ld

LDFLAGS += \
	-Xlinker --entry=__start \
	-nostartfiles \
	-nodefaultlibs \
	-Wl,--gc-sections \
	-u spresense_main

$(MKSPK): $(BUILD)/$(PROJECT).elf
	$(MAKE) -C $(TOP)/hw/mcu/sony/cxd56/mkspk

$(BUILD)/$(PROJECT).spk: $(MKSPK)
	@echo CREATE $@
	@$(MKSPK) -c 2 $(BUILD)/$(PROJECT).elf nuttx $@

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4F
