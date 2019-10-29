CFLAGS += \
  -D__MSP430F5529__ \
  -DCFG_TUSB_MCU=OPT_MCU_MSP430x5xx \
	-DCFG_EXAMPLE_MSC_READONLY \
	-DCFG_TUD_ENDPOINT0_SIZE=8

#-mmcu=msp430f5529

# All source paths should be relative to the top level.
LD_FILE = hw/mcu/ti/msp430/msp430-gcc-support-files/include/msp430f5529.ld
LDINC += $(TOP)/hw/mcu/ti/msp430/msp430-gcc-support-files/include

INC += $(TOP)/hw/mcu/ti/msp430/msp430-gcc-support-files/include

# For TinyUSB port source
VENDOR = ti
CHIP_FAMILY = msp430x5xx

# Path to mspdebug, should be added into system path
MSPDEBUG = mspdebug

# flash target using mspdebug.
flash: $(BUILD)/$(BOARD)-firmware.elf
	$(MSPDEBUG) tilib "prog $<" --allow-fw-update
