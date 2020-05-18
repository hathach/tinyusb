CFLAGS += \
	-DCONFIG_HAVE_DOUBLE \
	-Dmain=spresense_main \
	-pipe \
	-std=gnu11 \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfpu=fpv4-sp-d16 \
	-mfloat-abi=hard \
	-mabi=aapcs \
	-fno-builtin \
	-fno-strength-reduce \
	-fomit-frame-pointer \
	-DCFG_TUSB_MCU=OPT_MCU_CXD56 \

# lwip/src/core/raw.c:334:43: error: declaration of 'recv' shadows a global declaration
CFLAGS += -Wno-error=shadow
 
SPRESENSE_SDK = $(TOP)/hw/mcu/sony/cxd56/spresense-exported-sdk

INC += \
	$(SPRESENSE_SDK)/nuttx/include \
	$(SPRESENSE_SDK)/nuttx/arch \
	$(SPRESENSE_SDK)/nuttx/arch/chip \
	$(SPRESENSE_SDK)/sdk/bsp/include \
	$(SPRESENSE_SDK)/sdk/bsp/include/sdk \

LIBS += \
	$(SPRESENSE_SDK)/sdk/libs/libapps.a \
	$(SPRESENSE_SDK)/sdk/libs/libsdk.a \

LD_FILE = hw/mcu/sony/cxd56/spresense-exported-sdk/nuttx/build/ramconfig.ld

LDFLAGS += \
	-Xlinker --entry=__start \
	-nostartfiles \
	-nodefaultlibs \
	-Wl,--defsym,__stack=_vectors+786432 \
	-Wl,--gc-sections \
	-u spresense_main \

# For TinyUSB port source
VENDOR = sony
CHIP_FAMILY = cxd56

$(BUILD)/$(BOARD)-firmware.spk: $(BUILD)/$(BOARD)-firmware.elf
	@echo CREATE $@
	@$(SPRESENSE_SDK)/sdk/tools/linux/mkspk -c 2 $^ nuttx $@

# flash
flash: $(BUILD)/$(BOARD)-firmware.spk
	@$(SPRESENSE_SDK)/sdk/tools/flash.sh -c /dev/ttyUSB0 $<
