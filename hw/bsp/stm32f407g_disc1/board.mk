CFLAGS += \
	-DHSE_VALUE=8000000 \
	-DCFG_TUSB_MCU=OPT_MCU_STM32F4 \
	-DSTM32F407xx \
	-mthumb \
	-mabi=aapcs-linux \
	-mcpu=cortex-m4 \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16 \
	-nostdlib -nostartfiles

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/stm32f407g_disc1/STM32F407VGTx_FLASH.ld

LDFLAGS += -mthumb -mcpu=cortex-m4

SRC_C += \
	hw/mcu/st/system-init/system_stm32f4xx.c

SRC_S += \
	hw/mcu/st/startup/stm32f4/startup_stm32f407xx.s

INC += \
	-I$(TOP)/hw/mcu/st/stm32lib/CMSIS/STM32F4xx/Include \
	-I$(TOP)/hw/mcu/st/cmsis

VENDOR = st
CHIP_FAMILY = stm32f4

JLINK_DEVICE = stm32f407vg

# Path to STM32 Cube Programmer CLI, should be added into system path 
STM32Prog = STM32_Programmer_CLI

# flash target using on-board stlink
flash: $(BUILD)/$(BOARD)-firmware.elf
	$(STM32Prog) --connect port=swd --write $< --go
