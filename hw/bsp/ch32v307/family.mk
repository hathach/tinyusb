# https://www.embecosm.com/resources/tool-chain-downloads/#riscv-stable
#CROSS_COMPILE ?= riscv32-unknown-elf-

# Toolchain from https://github.com/xpack-dev-tools/riscv-none-embed-gcc-xpack
CROSS_COMPILE ?= riscv-none-embed-

# Submodules
CH32V307_SDK = hw/mcu/wch/ch32v307
DEPS_SUBMODULES += $(CH32V307_SDK)

# WCH-SDK paths
CH32V307_SDK_SRC = $(CH32V307_SDK)/EVT/EXAM/SRC

include $(TOP)/$(BOARD_PATH)/board.mk

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
	-DBOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED

SRC_C += \
	src/portable/wch/ch32v307/dcd_usbhs.c \
	$(CH32V307_SDK_SRC)/Core/core_riscv.c \
	$(CH32V307_SDK_SRC)/Peripheral/src/ch32v30x_gpio.c \
	$(CH32V307_SDK_SRC)/Peripheral/src/ch32v30x_misc.c \
	$(CH32V307_SDK_SRC)/Peripheral/src/ch32v30x_rcc.c \
	$(CH32V307_SDK_SRC)/Peripheral/src/ch32v30x_usart.c 
	
SRC_S += \
	$(CH32V307_SDK_SRC)/Startup/startup_ch32v30x_D8C.S 

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(CH32V307_SDK_SRC)/Peripheral/inc

# For freeRTOS port source
FREERTOS_PORT = RISC-V

# wch-link is not supported yet in official openOCD yet. We need to either use
# 1. download openocd as part of mounriver studio http://www.mounriver.com/download or
# 2. compiled from modified source https://github.com/kprasadvnsi/riscv-openocd-wch
#
# Note: For Linux, somehow openocd in mounriver studio does not seem to have wch-link enable, 
# therefore we need to compile it from source as follows:
# 	git clone https://github.com/kprasadvnsi/riscv-openocd-wch
# 	cd riscv-openocd-wch
#		./bootstrap
#		./configure CFLAGS="-Wno-error" --enable-wlink
#		make
# openocd binaries will be generated in riscv-openocd-wch/src

# flash target ROM bootloader
flash: $(BUILD)/$(PROJECT).elf
	openocd -f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg -c init -c halt -c "program $<" -c wlink_reset_resume -c exit
