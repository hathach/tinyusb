ST_FAMILY = l4
DEPS_SUBMODULES += lib/CMSIS_5 hw/mcu/st/cmsis_device_$(ST_FAMILY) hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

ST_CMSIS = hw/mcu/st/cmsis_device_$(ST_FAMILY)
ST_HAL_DRIVER = hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -DSTM32L476xx \
  -DCFG_TUSB_MCU=OPT_MCU_STM32L4

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=maybe-uninitialized -Wno-error=cast-align

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32L476VGTx_FLASH.ld

SRC_C += \
	src/portable/st/synopsys/dcd_synopsys.c \
	$(ST_CMSIS)/Source/Templates/system_stm32$(ST_FAMILY)xx.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_pwr.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_pwr_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart.c

SRC_S += \
	$(ST_CMSIS)/Source/Templates/gcc/startup_stm32l476xx.s

INC += \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc \
	$(TOP)/hw/bsp/$(BOARD)

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = stm32l476vg

# Path to STM32 Cube Programmer CLI, should be added into system path
STM32Prog = STM32_Programmer_CLI

# flash target using on-board stlink
flash: $(BUILD)/$(PROJECT).elf
	$(STM32Prog) --connect port=swd --write $< --go
