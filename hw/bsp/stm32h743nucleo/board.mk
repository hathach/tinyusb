CFLAGS += \
  -DHSE_VALUE=8000000 \
  -DSTM32H743xx \
  -mthumb \
  -mabi=aapcs-linux \
  -mcpu=cortex-m7 \
  -mfloat-abi=hard \
  -mfpu=fpv5-d16 \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_STM32H7 \
  -Wno-error=sign-compare

# The -Wno-error=sign-compare line is required due to STM32H7xx_HAL_Driver.

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/stm32h743nucleo/STM32H743ZITx_FLASH.ld

SRC_C += \
	hw/mcu/st/system-init/system_stm32h7xx.c \
	hw/mcu/st/stm32lib/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c \
	hw/mcu/st/stm32lib/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c \
	hw/mcu/st/stm32lib/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c \
	hw/mcu/st/stm32lib/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c

SRC_S += \
	hw/mcu/st/startup/stm32h7/startup_stm32h743xx.s

INC += \
	$(TOP)/hw/mcu/st/cmsis \
	$(TOP)/hw/mcu/st/stm32lib/CMSIS/STM32H7xx/Include \
	$(TOP)/hw/mcu/st/stm32lib/STM32H7xx_HAL_Driver/Inc \
	$(TOP)/hw/bsp/stm32h743nucleo

# For TinyUSB port source
VENDOR = st
CHIP_FAMILY = stm32h7
