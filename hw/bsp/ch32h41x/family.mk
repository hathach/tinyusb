# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

CH32_FAMILY = ch32h417
SDK_DIR = hw/mcu/wch/$(CH32_FAMILY)
SDK_SRC_DIR = $(SDK_DIR)/EVT/EXAM/SRC

include $(TOP)/$(BOARD_PATH)/board.mk

# In actual fact, it is IMACBF
CPU_CORE ?= rv32imac-ilp32

CFG_TUD_WCH_USBSS_DEBUG ?= 1
SPEED ?= super

CFLAGS += \
	-flto \
	-msmall-data-limit=8 \
	-mno-save-restore \
	-fmessage-length=0 \
	-fsigned-char \
	-DCH32H417 \
	-DCFG_TUSB_MCU=OPT_MCU_CH32H41X

CFLAGS += \
	-DCFG_TUD_WCH_USBIP_USBSS=1 \
	-DCFG_TUD_WCH_USBSS_DEBUG=$(CFG_TUD_WCH_USBSS_DEBUG) \
	-DBOARD_TUD_MAX_SPEED=OPT_MODE_SUPER_SPEED \
	-DCFG_TUD_ENDPOINT0_SIZE=512

SRC_C += \
	src/portable/wch/dcd_ch32_usbss.c


LDFLAGS += \
	-nostdlib -nostartfiles \
	--specs=nosys.specs --specs=nano.specs \

SRC_C += \
	$(SDK_SRC_DIR)/Core/core_riscv.c \
	$(SDK_SRC_DIR)/Peripheral/src/$(CH32_FAMILY)_flash.c \
	$(SDK_SRC_DIR)/Peripheral/src/$(CH32_FAMILY)_gpio.c \
	$(SDK_SRC_DIR)/Peripheral/src/$(CH32_FAMILY)_rcc.c \
	$(SDK_SRC_DIR)/Peripheral/src/$(CH32_FAMILY)_usart.c \
	$(FAMILY_PATH)/system_$(CH32_FAMILY).c \

SRC_S += \
	$(SDK_SRC_DIR)/Startup/startup_$(CH32_FAMILY)_v3f.S \

INC += \
	$(TOP)/$(SDK_SRC_DIR)/Core \
	$(TOP)/$(SDK_SRC_DIR)/Peripheral/inc \
	$(TOP)/$(FAMILY_PATH) \
	$(TOP)/$(BOARD_PATH) \

LD_FILE ?= $(SDK_SRC_DIR)/Ld/V3F/Link_v3f.ld

OPENOCD_WCH_OPTION=-f $(TOP)/$(FAMILY_PATH)/wch-dual-core.cfg -c noload
flash: flash-openocd-wch
