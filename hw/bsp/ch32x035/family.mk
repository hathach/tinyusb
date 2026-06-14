# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

CH32_FAMILY = ch32x035
SDK_DIR = hw/mcu/wch/ch32x035
SDK_SRC_DIR = $(SDK_DIR)/EVT/EXAM/SRC

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= rv32imac-ilp32

CFLAGS += \
  -mcmodel=medany \
  -ffat-lto-objects \
  -flto \
  -DCFG_TUSB_MCU=OPT_MCU_CH32X035 \
  -DCFG_TUD_WCH_USBIP_USBFS=1 \

CFLAGS += -Wno-error=strict-prototypes

LDFLAGS += \
  -nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \

LD_FILE = $(FAMILY_PATH)/linker/${CH32_FAMILY}.ld

SRC_C += \
  src/portable/wch/dcd_ch32_usbfs.c \
  $(SDK_SRC_DIR)/Core/core_riscv.c \
  $(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_dbgmcu.c \
  $(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_gpio.c \
  $(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_misc.c \
  $(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_rcc.c \
  $(SDK_SRC_DIR)/Peripheral/src/${CH32_FAMILY}_usart.c \

SRC_S += $(SDK_SRC_DIR)/Startup/startup_${CH32_FAMILY}.S

INC += \
  $(TOP)/$(BOARD_PATH) \
  $(TOP)/$(SDK_SRC_DIR)/Core \
  $(TOP)/$(SDK_SRC_DIR)/Peripheral/inc \

FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V

OPENOCD_WCH_OPTION=-f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg
flash: flash-wlink-rs
