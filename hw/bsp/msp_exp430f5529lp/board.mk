CFLAGS += \
  -D__MSP430F5529__ \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_MSP430x5xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/msp_exp430f5529lp/msp430f5529.ld
LDFLAGS += -L$(TOP)/hw/bsp/$(BOARD)

INC += $(TOP)/hw/bsp/$(BOARD)

# For TinyUSB port source
VENDOR = ti
CHIP_FAMILY = msp430x5xx

# Path to STM32 Cube Programmer CLI, should be added into system path
MSPDEBUG = mspdebug

# flash target using on-board stlink
flash: $(BUILD)/$(BOARD)-firmware.elf
	$(MSPDEBUG) tilib "prog $<"
