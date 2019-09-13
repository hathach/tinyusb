CFLAGS += \
  -DHSE_VALUE=8000000 \
  -DSTM32L476xx \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_STM32L4

ST_HAL_DRIVER = hw/mcu/st/st_driver/STM32L4xx_HAL_Driver
ST_CMSIS = hw/mcu/st/st_driver/CMSIS/Device/ST/STM32L4xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32L476VGTx_FLASH.ld

SRC_C += \
	$(ST_CMSIS)/Source/Templates/system_stm32l4xx.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_pwr.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_pwr_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_gpio.c

SRC_S += \
	$(ST_CMSIS)/Source/Templates/gcc/startup_stm32l476xx.s

INC += \
	$(TOP)/hw/mcu/st/st_driver/CMSIS/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc \
	$(TOP)/hw/bsp/$(BOARD)

# For TinyUSB port source
VENDOR = st
CHIP_FAMILY = synopsys

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = stm32l476vg
JLINK_IF = swd

# Path to STM32 Cube Programmer CLI, should be added into system path
STM32Prog = STM32_Programmer_CLI

# flash target using on-board stlink
flash: $(BUILD)/$(BOARD)-firmware.elf
	$(STM32Prog) --connect port=swd --write $< --go
