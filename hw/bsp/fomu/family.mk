CFLAGS += \
  -flto \
  -march=rv32i \
  -mabi=ilp32 \
  -nostdlib \
  -DCFG_TUSB_MCU=OPT_MCU_VALENTYUSB_EPTRI

# Toolchain from https://github.com/xpack-dev-tools/riscv-none-embed-gcc-xpack
CROSS_COMPILE = riscv-none-embed-

# All source paths should be relative to the top level.
LD_FILE = $(FAMILY_PATH)/fomu.ld

SRC_C += src/portable/valentyusb/eptri/dcd_eptri.c

SRC_S += $(FAMILY_PATH)/crt0-vexriscv.S

INC += \
	$(TOP)/$(FAMILY_PATH)/include

# For freeRTOS port source
FREERTOS_PORT = RISC-V

# flash using dfu-util
$(BUILD)/$(PROJECT).dfu: $(BUILD)/$(PROJECT).bin
	@echo "Create $@"
	python $(TOP)/hw/bsp/$(BOARD)/dfu.py -b $^ -D 0x1209:0x5bf0 $@
	
flash: $(BUILD)/$(PROJECT).dfu
	dfu-util -D $^
