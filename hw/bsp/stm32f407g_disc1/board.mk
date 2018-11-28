CFLAGS = \
	-DCFG_TUSB_MCU=OPT_MCU_STM32F4 \
	-DSTM32F407xx \
	-mthumb \
	-mabi=aapcs-linux \
	-mcpu=cortex-m4 \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/stm32f407g_disc1/STM32F407VGTx_FLASH.ld

LDFLAGS += -mthumb -mcpu=cortex-m4

SRC_C += \
	hw/mcu/stm/stm32f4/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c

SRC_S += \
	hw/mcu/stm/stm32f4/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f407xx.s

INC += \
	-I$(TOP)/hw/mcu/stm/stm32f4/Device/ST/STM32F4xx/Include \
	-I$(TOP)/hw/mcu/stm/stm32f4/Include

VENDOR = stm
CHIP_FAMILY = stm32f4
