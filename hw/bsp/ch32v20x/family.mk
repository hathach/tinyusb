# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

# Submodules
CH32V20X_SDK = hw/mcu/wch/ch32v20x
DEPS_SUBMODULES += $(CH32V20X_SDK)

# WCH-SDK paths
CH32V20X_SDK_SRC = $(CH32V20X_SDK)/EVT/EXAM/SRC

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
	-march=rv32imac_zicsr \
	-mabi=ilp32 \
	-mcmodel=medany \
	-ffunction-sections \
	-fdata-sections \
	-ffat-lto-objects \
	-flto \
	-nostdlib -nostartfiles \
	-DCFG_TUSB_MCU=OPT_MCU_CH32V20X \
	-DBOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED \

LDFLAGS_GCC += \
	-Wl,--gc-sections \
	-specs=nosys.specs \
	-specs=nano.specs \

LD_FILE = $(CH32V20X_SDK_SRC)/Ld/Link.ld

SRC_C += \
	src/portable/wch/dcd_ch32_usbfs.c \
	$(CH32V20X_SDK_SRC)/Core/core_riscv.c \
	$(CH32V20X_SDK_SRC)/Peripheral/src/ch32v20x_gpio.c \
	$(CH32V20X_SDK_SRC)/Peripheral/src/ch32v20x_misc.c \
	$(CH32V20X_SDK_SRC)/Peripheral/src/ch32v20x_rcc.c \
	$(CH32V20X_SDK_SRC)/Peripheral/src/ch32v20x_usart.c \

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(CH32V20X_SDK_SRC)/Peripheral/inc \

FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V

# wch-link is not supported yet in official openOCD yet. We need to either use
# 1. download openocd as part of mounriver studio http://www.mounriver.com/download or
# 2. compiled from modified source https://github.com/dragonlock2/miscboards/blob/main/wch/SDK/riscv-openocd.tar.xz
flash: $(BUILD)/$(PROJECT).elf
	openocd -f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg -c init -c halt -c "flash write_image $<" -c reset -c exit
