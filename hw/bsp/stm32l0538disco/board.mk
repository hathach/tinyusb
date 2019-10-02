CFLAGS += \
	-DHSE_VALUE=8000000 \
	-DSTM32L053xx \
	-mthumb \
	-mabi=aapcs \
	-mcpu=cortex-m0plus \
	-mfloat-abi=soft \
	-nostdlib -nostartfiles \
	-DCFG_EXAMPLE_MSC_READONLY \
	-DCFG_TUSB_MCU=OPT_MCU_STM32L0

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=maybe-uninitialized

ST_HAL_DRIVER = hw/mcu/st/st_driver/STM32L0xx_HAL_Driver
ST_CMSIS = hw/mcu/st/st_driver/CMSIS/Device/ST/STM32L0xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32L053C8Tx_FLASH.ld

SRC_C += \
	$(ST_CMSIS)/Source/Templates/system_stm32l0xx.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal_gpio.c

SRC_S += \
	$(ST_CMSIS)/Source/Templates/gcc/startup_stm32l053xx.s

INC += \
	$(TOP)/hw/mcu/st/st_driver/CMSIS/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc \
	$(TOP)/hw/bsp/$(BOARD)

# For TinyUSB port source
VENDOR = st
CHIP_FAMILY = stm32_fsdev

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = STM32L053R8
JLINK_IF = swd

# Path to STM32 Cube Programmer CLI, should be added into system path 
STM32Prog = STM32_Programmer_CLI

# flash target using on-board stlink
flash: $(BUILD)/$(BOARD)-firmware.elf
	$(STM32Prog) --connect port=swd --write $< --go
