CROSS_COMPILE = riscv32-unknown-elf-

DEPS_SUBMODULES += hw/mcu/gd/nuclei-sdk

NUCLEI_SDK = hw/mcu/gd/nuclei-sdk
GD32VF103_SDK_SOC_COMMON = $(NUCLEI_SDK)/SoC/gd32vf103/Common
GD32VF103_SDK_DRIVER = $(GD32VF103_SDK_SOC_COMMON)/Source/Drivers
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
#CFLAGS += -Wno-error=unused-parameter

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/gcc_gd32vf103xb_flashxip.ld # Longan Nano 128k ROM 32k RAM
# LD_FILE = hw/bsp/$(BOARD)/gcc_gd32vf103x8_flashxip.ld # Longan Nano Lite 64k ROM 20k RAM 

SRC_C += \
  src/portable/st/synopsys/dcd_synopsys.c \
	$(GD32VF103_SDK_DRIVER)/gd32vf103_rcu.c \
	$(GD32VF103_SDK_DRIVER)/gd32vf103_gpio.c \
	$(GD32VF103_SDK_DRIVER)/Usb/gd32vf103_usb_hw.c \
	$(GD32VF103_SDK_DRIVER)/gd32vf103_usart.c \
	$(GD32VF103_SDK_SOC_COMMON)/Source/Stubs/sbrk.c \
	$(GD32VF103_SDK_SOC_COMMON)/Source/Stubs/close.c \
	$(GD32VF103_SDK_SOC_COMMON)/Source/Stubs/isatty.c \
	$(GD32VF103_SDK_SOC_COMMON)/Source/Stubs/fstat.c \
	$(GD32VF103_SDK_SOC_COMMON)/Source/Stubs/lseek.c \
	$(GD32VF103_SDK_SOC_COMMON)/Source/Stubs/read.c 

SRC_S += \
  $(GD32VF103_SDK_SOC_COMMON)/Source/GCC/startup_gd32vf103.S \
  $(GD32VF103_SDK_SOC_COMMON)/Source/GCC/intexc_gd32vf103.S

INC += \
  $(TOP)/hw/bsp/$(BOARD) \
	$(TOP)/$(NUCLEI_SDK)/NMSIS/Core/Include \
	$(TOP)/$(GD32VF103_SDK_SOC_COMMON)/Include \
	$(TOP)/$(GD32VF103_SDK_SOC_COMMON)/Include/Usb \

# For freeRTOS port source
#FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = gd32vf103cbt6 # Longan Nano 128k ROM 32k RAM
#JLINK_DEVICE = gd32vf103c8t6 # Longan Nano Lite 64k ROM 20k RAM

# flash target ROM bootloader
flash: $(BUILD)/$(PROJECT).bin
	dfu-util -R -a 0 --dfuse-address 0x08000000 -D $<
