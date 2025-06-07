# https://www.embecosm.com/resources/tool-chain-downloads/#riscv-stable
#CROSS_COMPILE ?= riscv32-unknown-elf-

# Toolchain from https://nucleisys.com/download.php
#CROSS_COMPILE ?= riscv-nuclei-elf-

# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

CH32_FAMILY = ch32x035
SDK_DIR = hw/mcu/wch/ch32x035 # Replaced ch32v307
SDK_SRC_DIR = $(SDK_DIR)/EVT/EXAM/SRC

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= rv32imac-ilp32 # Assuming CPU core remains the same, verify if ch32x035 has a different core/extensions

# default to use high speed port, unless specified in board.mk or command line
# TODO: Verify if ch32x035 has USBHS, and if this SPEED variable is still applicable.
# For now, keeping logic, but it might need adjustment.
SPEED ?= high

CFLAGS += \
	-flto \
	-msmall-data-limit=8 \
	-mno-save-restore \
	-fmessage-length=0 \
	-fsigned-char \
	-DCFG_TUSB_MCU=OPT_MCU_CH32X035 # Replaced OPT_MCU_CH32V307

# https://github.com/openwch/ch32v307/pull/90  // TODO: Update this link if a ch32x035 equivalent exists
CFLAGS += -Wno-error=strict-prototypes

ifeq ($(SPEED),high)
  $(info "Using USBHS driver for HighSpeed mode")
  CFLAGS += -DCFG_TUD_WCH_USBIP_USBHS=1 # Keep if USBHS is relevant for ch32x035
else
  $(info "Using USBFS driver for FullSpeed mode")
  CFLAGS += -DCFG_TUD_WCH_USBIP_USBFS=1 # Keep if USBFS is relevant for ch32x035
endif

LDFLAGS_GCC += \
	-nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \

SRC_C += \
	src/portable/wch/dcd_ch32_usbhs.c \
	src/portable/wch/dcd_ch32_usbfs.c \
	$(SDK_SRC_DIR)/Core/core_riscv.c \
	$(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_gpio.c \
	$(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_misc.c \
	$(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_rcc.c \
	$(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_usart.c
# TODO: Verify peripheral driver names for ch32x035. They might differ from ch32v30x.

SRC_S += \
	$(SDK_SRC_DIR)/Startup/startup_${CH32_FAMILY}_D8C.S # Verify startup file name for ch32x035. _D8C might be specific.

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(SDK_SRC_DIR)/Core \
	$(TOP)/$(SDK_SRC_DIR)/Peripheral/inc
# TODO: Verify include paths for ch32x035 SDK structure.

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V

OPENOCD_WCH_OPTION=-f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg # TODO: Verify if ch32x035 uses a different openocd cfg.
flash: flash-openocd-wch
