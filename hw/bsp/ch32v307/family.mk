# https://www.embecosm.com/resources/tool-chain-downloads/#riscv-stable
#CROSS_COMPILE ?= riscv32-unknown-elf-

# Toolchain from https://github.com/xpack-dev-tools/riscv-none-embed-gcc-xpack
CROSS_COMPILE ?= riscv-none-embed-

# Submodules
CH32V307_SDK = hw/mcu/wch/ch32v307
DEPS_SUBMODULES += $(CH32V307_SDK)

# WCH-SDK paths
CH32V307_SDK_SRC = $(CH32V307_SDK)/EVT/EXAM/SRC
CH32V307_SDK_SRC_TOP = $(TOP)/$(CH32V307_SDK_SRC)
CH32V307_STARTUP_ASM = $(CH32V307_SDK_SRC)/Startup

include $(TOP)/$(BOARD_PATH)/board.mk

SKIP_NANOLIB = 1

CFLAGS += \
	-flto \
	-march=rv32imac \
	-mabi=ilp32 \
	-msmall-data-limit=8 \
	-mno-save-restore -Os \
	-fmessage-length=0 \
	-fsigned-char \
	-ffunction-sections \
	-fdata-sections \
	-nostdlib -nostartfiles \
	-DCFG_TUSB_MCU=OPT_MCU_CH32V307 \
	-Xlinker --gc-sections \
	-DBOARD_DEVICE_RHPORT_SPEED=OPT_MODE_HIGH_SPEED

# caused by extra void USART_Printf_Init() in debug_uart.h and EVT/EXAME/SRC/DEBUG/debug.h
CFLAGS += -Wno-error=redundant-decls

LDFLAGS += \
  -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

SRC_C += \
	src/portable/wch/ch32v307/usb_dc_usbhs.c \
	$(CH32V307_SDK_SRC_TOP)/Core/core_riscv.c \
	$(CH32V307_SDK_SRC_TOP)/Peripheral/src/ch32v30x_gpio.c \
	$(CH32V307_SDK_SRC_TOP)/Peripheral/src/ch32v30x_misc.c \
	$(CH32V307_SDK_SRC_TOP)/Peripheral/src/ch32v30x_rcc.c \
	$(CH32V307_SDK_SRC_TOP)/Peripheral/src/ch32v30x_usart.c 
	
SRC_S += \
	$(CH32V307_STARTUP_ASM)/startup_ch32v30x_D8C.S 

INC += \
	src/portable/wch/ch32v307 \
	$(TOP)/$(BOARD_PATH)/.. \
	$(TOP)/$(BOARD_PATH) \
	$(CH32V307_SDK_SRC_TOP)/Peripheral/inc \
	$(CH32V307_SDK_SRC_TOP)/Debug \
	$(CH32V307_SDK_SRC_TOP)

# For freeRTOS port source
FREERTOS_PORT = RISC-V

# flash target ROM bootloader
flash: $(BUILD)/$(PROJECT).elf
	openocd -f wch-riscv.cfg  -c init -c halt  -c "program  $<  0x08000000"  -c reset -c exit
