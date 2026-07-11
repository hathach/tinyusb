# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

SDK_DIR = hw/mcu/wch/ch32h417/EVT/EXAM/SRC
# system_ch32h417.c (clock setup) is a per-project file in the EVT; use the plain GPIO demo's V3F
# copy (SYSCLK 400 MHz, V3F core 100 MHz from the 25 MHz HSE - the vendor default all USB demos run)
SDK_SYSTEM_FILE = hw/mcu/wch/ch32h417/EVT/EXAM/GPIO/GPIO_Toggle/V3F/User/system_ch32h417.c

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= rv32imac-ilp32

# Controller selection: super = USB3.0 SuperSpeed (USBSS), high = USB2.0 HighSpeed (USBHS)
SPEED ?= super

# TinyUSB runs entirely on the V3F core (core 0, the boot core): single image, single link,
# standard flow. The V5F core (core 1) is left parked.
CFLAGS += \
	-flto \
	-msmall-data-limit=16 \
	-mno-save-restore \
	-fmessage-length=0 \
	-fsigned-char \
	-DCFG_TUSB_MCU=OPT_MCU_CH32H417 \
	-DCore_V3F=1 \
	-Wno-error=strict-prototypes \
	-Wno-comment

ifeq ($(SPEED),super)
  # USB3 SuperSpeed with runtime USB2 high-speed fallback (uses TIM12 as training timeout).
  # Set CFG_TUD_WCH_USB30_FALLBACK=0 for SuperSpeed-only (trains indefinitely)
  CFG_TUD_WCH_USB30_FALLBACK ?= 1
  CFLAGS += -DCFG_TUD_WCH_USBIP_USB30=1 -DCFG_TUD_WCH_USB30_FALLBACK=$(CFG_TUD_WCH_USB30_FALLBACK)
else
  CFLAGS += -DCFG_TUD_WCH_USBIP_USBHS=1
endif

LDFLAGS += \
	-nostartfiles \
	--specs=nosys.specs --specs=nano.specs

SRC_C += \
	src/portable/wch/dcd_ch32h417_usb30.c \
	src/portable/wch/dcd_ch32h417_usbhs.c \
	$(SDK_DIR)/Core/core_riscv.c \
	$(SDK_DIR)/Peripheral/src/ch32h417_dbgmcu.c \
	$(SDK_DIR)/Peripheral/src/ch32h417_flash.c \
	$(SDK_DIR)/Peripheral/src/ch32h417_gpio.c \
	$(SDK_DIR)/Peripheral/src/ch32h417_rcc.c \
	$(SDK_DIR)/Peripheral/src/ch32h417_usart.c \
	$(SDK_SYSTEM_FILE)

SRC_S += \
	$(SDK_DIR)/Startup/startup_ch32h417_v3f.S

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(FAMILY_PATH) \
	$(TOP)/$(SDK_DIR)/Core \
	$(TOP)/$(SDK_DIR)/Peripheral/inc

LD_FILE ?= $(FAMILY_PATH)/linker/ch32h417_v3f.ld

OPENOCD_WCH_OPTION=-f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg
flash: flash-openocd-wch

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V
