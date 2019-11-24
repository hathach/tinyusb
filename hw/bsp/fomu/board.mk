CFLAGS += \
  -flto \
  -march=rv32i \
  -mabi=ilp32 \
  -nostdlib \
  -DCFG_TUSB_MCU=OPT_MCU_VALENTYUSB_EPTRI

MCU_DIR = hw/mcu/fomu
BSP_DIR = hw/bsp/fomu

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/fomu/fomu.ld

# TODO remove later
SRC_C += src/portable/$(VENDOR)/$(CHIP_FAMILY)/hal_$(CHIP_FAMILY).c

SRC_C += 

SRC_S += hw/bsp/fomu/crt0-vexriscv.S

INC += \
	$(TOP)/$(BSP_DIR)/include

# For TinyUSB port source
VENDOR = valentyusb
CHIP_FAMILY = eptri

# flash using dfu-util
flash: $(BUILD)/$(BOARD)-firmware.dfu
	dfu-util -D $^
