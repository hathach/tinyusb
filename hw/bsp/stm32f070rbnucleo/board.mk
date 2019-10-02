CFLAGS += \
	-DHSE_VALUE=8000000 \
	-DSTM32F070xB \
	-mthumb \
	-mabi=aapcs-linux \
	-mcpu=cortex-m0 \
	-mfloat-abi=soft \
	-nostdlib -nostartfiles \
	-DCFG_EXAMPLE_MSC_READONLY \
	-DCFG_TUSB_MCU=OPT_MCU_STM32F0

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter

ST_HAL_DRIVER = hw/mcu/st/st_driver/STM32F0xx_HAL_Driver
ST_CMSIS = hw/mcu/st/st_driver/CMSIS/Device/ST/STM32F0xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/stm32F070rbtx_flash.ld

SRC_C += \
	$(ST_CMSIS)/Source/Templates/system_stm32f0xx.c \
	$(ST_HAL_DRIVER)/Src/stm32f0xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32f0xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32f0xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32f0xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32f0xx_hal_gpio.c

SRC_S += \
	$(ST_CMSIS)/Source/Templates/gcc/startup_stm32f070xb.s

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
JLINK_DEVICE = stm32f070rb
JLINK_IF = swd

# Path to STM32 Cube Programmer CLI, should be added into system path 
STM32Prog = STM32_Programmer_CLI

# flash target using on-board stlink
flash: $(BUILD)/$(BOARD)-firmware.elf
	$(STM32Prog) --connect port=swd --write $< --go
