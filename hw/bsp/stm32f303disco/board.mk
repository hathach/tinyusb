CFLAGS += \
	-DHSE_VALUE=8000000 \
	-DSTM32F303xC \
	-mthumb \
	-mabi=aapcs \
	-mcpu=cortex-m4 \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16 \
	-nostdlib -nostartfiles \
	-DCFG_TUSB_MCU=OPT_MCU_STM32F3

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter

ST_HAL_DRIVER = hw/mcu/st/st_driver/STM32F3xx_HAL_Driver
ST_CMSIS = hw/mcu/st/st_driver/CMSIS/Device/ST/STM32F3xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32F303VCTx_FLASH.ld

SRC_C += \
	$(ST_CMSIS)/Source/Templates/system_stm32f3xx.c \
	$(ST_HAL_DRIVER)/Src/stm32f3xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32f3xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32f3xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32f3xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32f3xx_hal_gpio.c

SRC_S += \
	$(ST_CMSIS)/Source/Templates/gcc/startup_stm32f303xc.s

INC += \
	$(TOP)/hw/mcu/st/st_driver/CMSIS/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc \
	$(TOP)/hw/bsp/$(BOARD)

# For TinyUSB port source
VENDOR = st
CHIP_FAMILY = stm32_fsdev

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = stm32f303vc
JLINK_IF = swd

# Path to STM32 Cube Programmer CLI, should be added into system path 
STM32Prog = STM32_Programmer_CLI

# flash target using on-board stlink
flash: $(BUILD)/$(BOARD)-firmware.elf
	$(STM32Prog) --connect port=swd --write $< --go
