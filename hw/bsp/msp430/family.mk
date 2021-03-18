CROSS_COMPILE = msp430-elf-
DEPS_SUBMODULES += hw/mcu/ti
SKIP_NANOLIB = 1

CFLAGS += \
  -D__MSP430F5529__ \
  -DCFG_TUSB_MCU=OPT_MCU_MSP430x5xx \
	-DCFG_EXAMPLE_MSC_READONLY \
	-DCFG_TUD_ENDPOINT0_SIZE=8

# All source paths should be relative to the top level.
LD_FILE = hw/mcu/ti/msp430/msp430-gcc-support-files/include/msp430f5529.ld
LDINC += $(TOP)/hw/mcu/ti/msp430/msp430-gcc-support-files/include
LDFLAGS += $(addprefix -L,$(LDINC))

SRC_C += src/portable/ti/msp430x5xx/dcd_msp430x5xx.c

INC += \
	$(TOP)/hw/mcu/ti/msp430/msp430-gcc-support-files/include \
	$(TOP)/$(BOARD_PATH)

# export for libmsp430.so to same installation
ifneq ($(OS),Windows_NT)
export LD_LIBRARY_PATH=$(dir $(shell which MSP430Flasher))
endif

# flash target using TI MSP430-Flasher
# http://www.ti.com/tool/MSP430-FLASHER
# Please add its installation dir to PATH
flash: $(BUILD)/$(PROJECT).hex
	MSP430Flasher -w $< -z [VCC]

# flash target using mspdebug.
flash-mspdebug: $(BUILD)/$(PROJECT).elf
	$(MSPDEBUG) tilib "prog $<" --allow-fw-update
