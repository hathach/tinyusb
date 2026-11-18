# https://www.embecosm.com/resources/tool-chain-downloads/#riscv-stable
#CROSS_COMPILE ?= riscv32-unknown-elf-

# Toolchain from https://nucleisys.com/download.php
#CROSS_COMPILE ?= riscv-nuclei-elf-

# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

CH32_FAMILY = ch32v30x
SDK_DIR = hw/mcu/wch/ch32v307
SDK_SRC_DIR = $(SDK_DIR)/EVT/EXAM/SRC

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= rv32imac-ilp32

# QingKe RTT lock for LOGGER=rtt, see the header
FAMILY_RTT_CONF = $(TOP)/$(FAMILY_PATH)/rtt_conf_qingke.h

RHPORT_HOST ?= 1
CFLAGS += -DBOARD_TUH_RHPORT=$(RHPORT_HOST)

WCH_HOST_USBIP ?= usbfs
ifeq ($(WCH_HOST_USBIP),usbhs)
  CFLAGS += -DCFG_TUH_WCH_USBIP_USBHS=1
  BOARD_TUH_MAX_SPEED ?= OPT_MODE_HIGH_SPEED
  SPEED ?= full
else ifeq ($(WCH_HOST_USBIP),usbfs)
  BOARD_TUH_MAX_SPEED ?= OPT_MODE_FULL_SPEED
  SPEED ?= high
else
  $(error WCH_HOST_USBIP must be usbfs or usbhs)
endif
CFLAGS += -DBOARD_TUH_MAX_SPEED=$(BOARD_TUH_MAX_SPEED)

CFLAGS += \
	-flto \
	-msmall-data-limit=8 \
	-mno-save-restore \
	-fmessage-length=0 \
	-fsigned-char \
	-DCFG_TUSB_MCU=OPT_MCU_CH32V307 \

# https://github.com/openwch/ch32v307/pull/90
CFLAGS += -Wno-error=strict-prototypes

ifeq ($(SPEED),high)
  $(info "Using USBHS driver for HighSpeed mode")
  CFLAGS += -DCFG_TUD_WCH_USBIP_USBHS=1 -DBOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
else
  $(info "Using USBFS driver for FullSpeed mode")
  CFLAGS += -DCFG_TUD_WCH_USBIP_USBFS=1 -DBOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
endif

LDFLAGS += \
	-nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \

SRC_C += \
	src/portable/wch/usbhs_v1/dcd_wch_usbhs_v1.c \
	src/portable/wch/usbhs_v1/hcd_wch_usbhs_v1.c \
	src/portable/wch/usbfs/dcd_wch_usbfs.c \
	src/portable/wch/usbfs/hcd_wch_usbfs.c \
	$(SDK_SRC_DIR)/Core/core_riscv.c \
	$(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_gpio.c \
	$(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_misc.c \
	$(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_rcc.c \
	$(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_usart.c

SRC_S += \
	$(SDK_SRC_DIR)/Startup/startup_${CH32_FAMILY}_D8C.S

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(SDK_SRC_DIR)/Core \
	$(TOP)/$(SDK_SRC_DIR)/Peripheral/inc

LD_FILE ?= $(FAMILY_PATH)/linker/ch32v30x.ld

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V

OPENOCD_OPTION=-f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg
flash: flash-openocd-wch
