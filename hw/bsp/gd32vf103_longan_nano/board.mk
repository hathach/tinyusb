CROSS_COMPILE = riscv32-unknown-elf-

# Submodules
DEPS_SUBMODULES += hw/mcu/gd/nuclei-sdk
NUCLEI_SDK = hw/mcu/gd/nuclei-sdk

# Nuclei-SDK paths
GD32VF103_SDK_SOC = $(NUCLEI_SDK)/SoC/gd32vf103
GD32VF103_SDK_DRIVER = $(GD32VF103_SDK_SOC)/Common/Source/Drivers
LONGAN_NANO_SDK_BSP = $(GD32VF103_SDK_SOC)/Board/gd32vf103c_longan_nano
LINKER_SCRIPTS = $(LONGAN_NANO_SDK_BSP)/Source/GCC
LIBC_STUBS = $(GD32VF103_SDK_SOC)/Common/Source/Stubs
STARTUP_ASM = $(GD32VF103_SDK_SOC)/Common/Source/GCC

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

# All source paths should be relative to the top level.
LD_FILE = $(LINKER_SCRIPTS)/gcc_gd32vf103xb_flashxip.ld # Longan Nano 128k ROM 32k RAM
#LD_FILE = $(LINKER_SCRIPTS)/gcc_gd32vf103x8_flashxip.ld # Longan Nano Lite 64k ROM 20k RAM 

SRC_C += \
	src/portable/st/synopsys/dcd_synopsys.c \
	$(GD32VF103_SDK_DRIVER)/gd32vf103_rcu.c \
	$(GD32VF103_SDK_DRIVER)/gd32vf103_gpio.c \
	$(GD32VF103_SDK_DRIVER)/Usb/gd32vf103_usb_hw.c \
	$(GD32VF103_SDK_DRIVER)/gd32vf103_usart.c \
	$(LONGAN_NANO_SDK_BSP)/Source/gd32vf103c_longan_nano.c \
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
	$(TOP)/hw/bsp/$(BOARD) \
	$(TOP)/$(NUCLEI_SDK)/NMSIS/Core/Include \
	$(TOP)/$(GD32VF103_SDK_SOC)/Common/Include \
	$(TOP)/$(GD32VF103_SDK_SOC)/Common/Include/Usb \
	$(TOP)/$(LONGAN_NANO_SDK_BSP)/Include

# For freeRTOS port source
FREERTOS_PORT = RISC-V

# For flash-jlink target
JLINK_IF = jtag
JLINK_DEVICE = gd32vf103cbt6 # Longan Nano 128k ROM 32k RAM
#JLINK_DEVICE = gd32vf103c8t6 # Longan Nano Lite 64k ROM 20k RAM

# flash target ROM bootloader
flash: $(BUILD)/$(PROJECT).bin
	dfu-util -R -a 0 --dfuse-address 0x08000000 -D $<
