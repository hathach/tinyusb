MCU_DIR = hw/mcu/broadcom
DEPS_SUBMODULES += $(MCU_DIR)

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
	-Wall \
	-O0 \
	-ffreestanding \
	-nostdlib \
	-nostartfiles \
	-mgeneral-regs-only \
	-fno-exceptions \
	-std=c17

CROSS_COMPILE = arm-none-eabi-

# mcu driver cause following warnings
CFLAGS += -Wno-error=cast-qual -Wno-error=redundant-decls

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	$(MCU_DIR)/broadcom/gen/interrupt_handlers.c \
	$(MCU_DIR)/broadcom/gpio.c \
	$(MCU_DIR)/broadcom/interrupts.c \
	$(MCU_DIR)/broadcom/mmu.c \
	$(MCU_DIR)/broadcom/caches.c \
	$(MCU_DIR)/broadcom/vcmailbox.c

SKIP_NANOLIB = 1

LD_FILE = $(MCU_DIR)/broadcom/link$(SUFFIX).ld

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(MCU_DIR)

SRC_S += $(MCU_DIR)/broadcom/boot$(SUFFIX).S

$(BUILD)/kernel$(SUFFIX).img: $(BUILD)/$(PROJECT).elf
	$(OBJCOPY) -O binary $^ $@

# Copy to kernel to netboot drive or SD card
# Change destinaation to fit your need
flash: $(BUILD)/kernel$(SUFFIX).img
	@$(CP) $< /home/$(USER)/Documents/code/pi_tinyusb/boot_cpy
