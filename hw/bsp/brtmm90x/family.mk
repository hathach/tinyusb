
CROSS_COMPILE = ft32-elf-
DEPS_SUBMODULES += hw/mcu/bridgetek/ft9xx/hardware
SKIP_NANOLIB = 1

# This is installed at "C:/Program Files(x86)/Bridgetek/FT9xx Toolchain/Toolchain/hardware"
FT9XX_SDK = $(TOP)/hw/mcu/bridgetek/ft9xx/hardware

CFLAGS += \
	-D__FT900__ \
	-fvar-tracking \
	-fvar-tracking-assignments \
	-fmessage-length=0 \
	-ffunction-sections \
	-DCFG_TUSB_MCU=OPT_MCU_FT90X

# lwip/src/core/raw.c:334:43: error: declaration of 'recv' shadows a global declaration
CFLAGS += -Wno-error=shadow
CFLAGS:=$(filter-out -Wcast-function-type,$(CFLAGS))

# All source paths should be relative to the top level.
LDINC += $(FT9XX_SDK)/lib/Release
LIBS += -lft900
LD_FILE = hw/mcu/bridgetek/ft9xx/hardware/scripts/ldscript.ld
LDFLAGS += $(addprefix -L,$(LDINC)) \
	-Xlinker --entry=_start \
	-Wl,-lc

SRC_C += src/portable/bridgetek/ft9xx/dcd_ft9xx.c 

#SRC_S += hw/mcu/bridgetek/ft9xx/hardware/scripts/crt0.S

INC += \
	$(FT9XX_SDK)/include \
	$(TOP)/$(BOARD_PATH)
