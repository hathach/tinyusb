# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

SDK_DIR = hw/mcu/wch/ch32v407_ch32v467/EVT/EXAM/SRC
# system_ch32v4x7.c is a per-project file in the EVT: use the GPIO demo's (SYSCLK 400 MHz, HCLK 200 MHz
# from the 25 MHz HSE)
SDK_SYSTEM_FILE = hw/mcu/wch/ch32v407_ch32v467/EVT/EXAM/GPIO/GPIO_Toggle/User/system_ch32v4x7.c

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= rv32imac-ilp32

# rhport 0 = USBHS1 (PA11/PA12), rhport 1 = USBHS2 (PB6/PB7)
RHPORT_DEVICE ?= 0

CFLAGS += \
	-flto \
	-msmall-data-limit=8 \
	-mno-save-restore \
	-fmessage-length=0 \
	-fsigned-char \
	-DCFG_TUSB_MCU=OPT_MCU_CH32V407 \
	-DBOARD_TUD_RHPORT=$(RHPORT_DEVICE) \
	-Wno-error=strict-prototypes

# SDK RCC_GetClocksFreq() leaves SYSCLK_Frequency unset on an unknown PLL source (flagged at LTO link)
CFLAGS += -Wno-error=maybe-uninitialized

LDFLAGS += \
	-nostartfiles \
	--specs=nosys.specs --specs=nano.specs

SRC_C += \
	src/portable/wch/wch_hs2/dcd_wch_hs2.c \
	$(SDK_DIR)/Core/core_riscv.c \
	$(SDK_DIR)/Peripheral/src/ch32v4x7_gpio.c \
	$(SDK_DIR)/Peripheral/src/ch32v4x7_rcc.c \
	$(SDK_DIR)/Peripheral/src/ch32v4x7_usart.c \
	$(SDK_SYSTEM_FILE)

SRC_S += \
	$(SDK_DIR)/Startup/startup_ch32v4x7.S

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(FAMILY_PATH) \
	$(TOP)/$(SDK_DIR)/Core \
	$(TOP)/$(SDK_DIR)/Peripheral/inc

LD_FILE ?= $(SDK_DIR)/Ld/Link.ld

flash: flash-wlink-rs

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V
