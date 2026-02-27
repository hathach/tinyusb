# https://www.embecosm.com/resources/tool-chain-downloads/#riscv-stable
#CROSS_COMPILE ?= riscv32-unknown-elf-

# Toolchain from https://nucleisys.com/download.php
#CROSS_COMPILE ?= riscv-nuclei-elf-

# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

SDK_DIR = hw/mcu/wch/ch58x
SDK_SRC_DIR = $(SDK_DIR)/EVT/EXAM/SRC

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= rv32imac-ilp32

CFLAGS += \
	-flto \
	-msmall-data-limit=16 \
	-mno-save-restore \
	-fmessage-length=0 \
	-fsigned-char \
	-DCFG_TUSB_MCU=OPT_MCU_CH58X \
	-DCFG_TUD_WCH_USBIP_USBFS=1 \
	-DFREQ_SYS=60000000 \
	-DDISK_LIB_ENABLE=0 \
	-DINT_SOFT \

CFLAGS += -Wno-error=strict-prototypes

LDFLAGS_GCC += \
	-nostdlib -nostartfiles \
	--specs=nosys.specs \

SRC_C += \
	src/portable/wch/dcd_ch58x_usbfs.c \
	src/portable/wch/hcd_ch58x_usbfs.c \
	$(SDK_SRC_DIR)/StdPeriphDriver/CH58x_gpio.c \
	$(SDK_SRC_DIR)/StdPeriphDriver/CH58x_clk.c \
	$(SDK_SRC_DIR)/StdPeriphDriver/CH58x_uart1.c \
	$(SDK_SRC_DIR)/StdPeriphDriver/CH58x_sys.c \
	$(FAMILY_PATH)/debug_uart.c \
	$(FAMILY_PATH)/ch58x_it.c \
	$(FAMILY_PATH)/system_ch58x.c \

SRC_S += \
	$(SDK_SRC_DIR)/Startup/startup_CH583.S

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(SDK_SRC_DIR)/RVMSIS \
	$(TOP)/$(SDK_SRC_DIR)/StdPeriphDriver/inc

LD_FILE ?= $(FAMILY_PATH)/linker/ch582.ld

OPENOCD_WCH_OPTION=-f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg
flash: flash-openocd-wch

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V
