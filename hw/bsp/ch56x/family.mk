# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

SDK_DIR = hw/mcu/wch/ch56x

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= rv32imac-ilp32

# Controller selection: super = USB3.0 SuperSpeed (USBSS), high = USB2.0 HighSpeed (USBHS)
SPEED ?= super

CFLAGS += \
	-flto \
	-msmall-data-limit=16 \
	-mno-save-restore \
	-fmessage-length=0 \
	-fsigned-char \
	-DCFG_TUSB_MCU=OPT_MCU_CH569 \
	-DFREQ_SYS=120000000 \
	-DCFG_TUSB_MEM_SECTION='__attribute__((section(".dmadata")))' \
	-DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(16)))' \
	-Wno-error=strict-prototypes \
	-Wno-comment

ifeq ($(SPEED),super)
  CFLAGS += -DCFG_TUD_WCH_USBIP_USB30=1
else
  CFLAGS += -DCFG_TUD_WCH_USBIP_USBHS=1
endif

LDFLAGS += \
	-nostartfiles \
	--specs=nosys.specs --specs=nano.specs

SRC_C += \
	src/portable/wch/dcd_ch56x_usb30.c \
	src/portable/wch/dcd_ch56x_usbhs.c \
	$(SDK_DIR)/drv/CH56x_clk.c \
	$(SDK_DIR)/drv/CH56x_gpio.c \
	$(SDK_DIR)/drv/CH56x_uart.c \
	$(SDK_DIR)/drv/CH56x_sys.c \
	$(SDK_DIR)/rvmsis/core_riscv.c \
	$(FAMILY_PATH)/debug_uart.c

SRC_S += \
	$(SDK_DIR)/startup/startup_CH56x.S

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(SDK_DIR)/drv \
	$(TOP)/$(SDK_DIR)/rvmsis

LD_FILE ?= $(FAMILY_PATH)/linker/ch569.ld

OPENOCD_WCH_OPTION=-f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg
flash: flash-openocd-wch

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V
