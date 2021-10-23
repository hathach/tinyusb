DEPS_SUBMODULES += lib/CMSIS_5 hw/mcu/ti

CFLAGS += \
	-flto \
	-mthumb \
	-mslow-flash-data \
	-mabi=aapcs \
	-mcpu=cortex-m4 \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16 \
	-D__MSP432E401Y__ \
	-DCFG_TUSB_MCU=OPT_MCU_MSP432E4xx

# All source paths should be relative to the top level.
LD_FILE = hw/mcu/ti/msp432e4/Source/msp432e401y.ld
LDINC += $(TOP)/hw/mcu/ti/msp432e4/Include
LDFLAGS += $(addprefix -L,$(LDINC))

MCU_DIR = hw/mcu/ti/msp432e4

SRC_C += \
	src/portable/mentor/dcd_musb.c \
	$(MCU_DIR)/Source/system_msp432e401y.c

INC += \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(MCU_DIR)/Include \
	$(TOP)/$(BOARD_PATH)

SRC_S += $(MCU_DIR)/Source/startup_msp432e411y_gcc.S

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

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
