UF2_FAMILY_ID = 0x57755a57

include $(TOP)/$(BOARD_PATH)/board.mk

MCU_DIR = hw/mcu/broadcom/bcm2711

CC = clang

CFLAGS += \
	-Wall \
	-O0 \
	-ffreestanding \
	-nostdlib \
	-nostartfiles \
	-std=c17 \
	-mgeneral-regs-only \
	-DCFG_TUSB_MCU=OPT_MCU_BCM2711

SRC_C += \
	src/portable/broadcom/synopsys/dcd_synopsys.c \
	$(MCU_DIR)/io.c \
	$(MCU_DIR)/interrupts.c

CROSS_COMPILE = aarch64-none-elf-

SKIP_NANOLIB = 1

LD_FILE = $(MCU_DIR)/link.ld

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(MCU_DIR)

SRC_S += $(MCU_DIR)/boot.S

$(BUILD)/kernel8.img: $(BUILD)/$(PROJECT).elf
	$(OBJCOPY) -O binary $^ $@
