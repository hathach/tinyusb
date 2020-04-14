CFLAGS += \
  -flto \
  -march=rv32i \
  -mabi=ilp32 \
  -nostdlib \
  -DCFG_TUSB_MCU=OPT_MCU_VALENTYUSB_EPTRI

# Cross Compiler for RISC-V
CROSS_COMPILE = riscv-none-embed-

MCU_DIR = hw/mcu/fomu
BSP_DIR = hw/bsp/fomu

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/fomu.ld

SRC_S += hw/bsp/fomu/crt0-vexriscv.S

INC += \
	$(TOP)/$(BSP_DIR)/include

# For TinyUSB port source
VENDOR = valentyusb
CHIP_FAMILY = eptri

# For freeRTOS port source
FREERTOS_PORT = RISC-V

# flash using dfu-util
flash: $(BUILD)/$(BOARD)-firmware.dfu
	dfu-util -D $^
