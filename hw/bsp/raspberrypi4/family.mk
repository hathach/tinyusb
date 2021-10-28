UF2_FAMILY_ID = 0x57755a57

include $(TOP)/$(BOARD_PATH)/board.mk

MCU_DIR = hw/mcu/broadcom

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

# mcu driver cause following warnings
CFLAGS += -Wno-error=cast-qual

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	$(MCU_DIR)/broadcom/gen/interrupt_handlers.c \
	$(MCU_DIR)/broadcom/interrupts.c \
	$(MCU_DIR)/broadcom/io.c \
	$(MCU_DIR)/broadcom/mmu.c \
	$(MCU_DIR)/broadcom/vcmailbox.c

CROSS_COMPILE = aarch64-none-elf-

SKIP_NANOLIB = 1

LD_FILE = $(MCU_DIR)/broadcom/link.ld

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core_A/Include

SRC_S += $(MCU_DIR)/broadcom/boot.S

$(BUILD)/kernel8.img: $(BUILD)/$(PROJECT).elf
	$(OBJCOPY) -O binary $^ $@

flash: $(BUILD)/kernel8.img
	@$(CP) $< /home/$(USER)/Documents/code/pi4_tinyusb/boot_cpy
