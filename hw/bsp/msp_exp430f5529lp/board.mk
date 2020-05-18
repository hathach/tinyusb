CFLAGS += \
  -D__MSP430F5529__ \
  -DCFG_TUSB_MCU=OPT_MCU_MSP430x5xx \
	-DCFG_EXAMPLE_MSC_READONLY \
	-DCFG_TUD_ENDPOINT0_SIZE=8

#-mmcu=msp430f5529

# Cross Compiler for MSP430
CROSS_COMPILE = msp430-elf-

# All source paths should be relative to the top level.
LD_FILE = hw/mcu/ti/msp430/msp430-gcc-support-files/include/msp430f5529.ld
LDINC += $(TOP)/hw/mcu/ti/msp430/msp430-gcc-support-files/include
LDFLAGS += $(addprefix -L,$(LDINC))

INC += $(TOP)/hw/mcu/ti/msp430/msp430-gcc-support-files/include

# For TinyUSB port source
VENDOR = ti
CHIP_FAMILY = msp430x5xx

# export for libmsp430.so to same installation
ifneq ($(OS),Windows_NT)
export LD_LIBRARY_PATH=$(dir $(shell which MSP430Flasher))
endif

# flash target using TI MSP430-Flasher
# http://www.ti.com/tool/MSP430-FLASHER
# Please add its installation dir to PATH
flash: $(BUILD)/$(BOARD)-firmware.hex
	MSP430Flasher -w $< -z [VCC]

# flash target using mspdebug.
flash-mspdebug: $(BUILD)/$(BOARD)-firmware.elf
	$(MSPDEBUG) tilib "prog $<" --allow-fw-update
