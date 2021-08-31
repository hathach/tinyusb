# https://www.embecosm.com/resources/tool-chain-downloads/#riscv-stable
#CROSS_COMPILE ?= riscv32-unknown-elf-

# Toolchain from https://nucleisys.com/download.php
#CROSS_COMPILE ?= riscv-nuclei-elf-

# Toolchain from https://github.com/xpack-dev-tools/riscv-none-embed-gcc-xpack
CROSS_COMPILE ?= riscv-none-embed-

# Submodules
NUCLEI_SDK = hw/mcu/gd/nuclei-sdk
DEPS_SUBMODULES += $(NUCLEI_SDK)

# Nuclei-SDK paths
GD32VF103_SDK_SOC = $(NUCLEI_SDK)/SoC/gd32vf103
GD32VF103_SDK_DRIVER = $(GD32VF103_SDK_SOC)/Common/Source/Drivers
LIBC_STUBS = $(GD32VF103_SDK_SOC)/Common/Source/Stubs
STARTUP_ASM = $(GD32VF103_SDK_SOC)/Common/Source/GCC

include $(TOP)/$(BOARD_PATH)/board.mk

SKIP_NANOLIB = 1

CFLAGS += \
	-march=rv32imac \
	-mabi=ilp32 \
	-mcmodel=medlow \
	-mstrict-align \
	-nostdlib -nostartfiles \
	-DCFG_TUSB_MCU=OPT_MCU_GD32VF103 \
	-DDOWNLOAD_MODE=DOWNLOAD_MODE_FLASHXIP \
	-DGD32VF103 

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter

SRC_C += \
	src/portable/st/synopsys/dcd_synopsys.c \
	$(GD32VF103_SDK_DRIVER)/gd32vf103_rcu.c \
	$(GD32VF103_SDK_DRIVER)/gd32vf103_gpio.c \
	$(GD32VF103_SDK_DRIVER)/Usb/gd32vf103_usb_hw.c \
	$(GD32VF103_SDK_DRIVER)/gd32vf103_usart.c \
	$(LIBC_STUBS)/sbrk.c \
	$(LIBC_STUBS)/close.c \
	$(LIBC_STUBS)/isatty.c \
	$(LIBC_STUBS)/fstat.c \
	$(LIBC_STUBS)/lseek.c \
	$(LIBC_STUBS)/read.c 

SRC_S += \
	$(STARTUP_ASM)/startup_gd32vf103.S \
	$(STARTUP_ASM)/intexc_gd32vf103.S

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(NUCLEI_SDK)/NMSIS/Core/Include \
	$(TOP)/$(GD32VF103_SDK_SOC)/Common/Include \
	$(TOP)/$(GD32VF103_SDK_SOC)/Common/Include/Usb

# For freeRTOS port source
FREERTOS_PORT = RISC-V

# For flash-jlink target
JLINK_IF = jtag

# flash target ROM bootloader
flash: $(BUILD)/$(PROJECT).bin
	dfu-util -R -a 0 --dfuse-address 0x08000000 -D $<
