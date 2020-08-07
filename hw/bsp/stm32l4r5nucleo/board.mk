CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -DHSE_VALUE=8000000 \
  -DSTM32L4R5xx \
  -DCFG_TUSB_MCU=OPT_MCU_STM32L4

# suppress warning caused by vendor mcu driver
CFLAGS +=  -Wno-error=maybe-uninitialized -Wno-error=cast-align

ST_HAL_DRIVER = hw/mcu/st/st_driver/STM32L4xx_HAL_Driver
ST_CMSIS = hw/mcu/st/st_driver/CMSIS/Device/ST/STM32L4xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32L4RXxI_FLASH.ld

SRC_C += \
	$(ST_CMSIS)/Source/Templates/system_stm32l4xx.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_uart.c \
	$(ST_HAL_DRIVER)/Src/stm32l4xx_hal_pwr_ex.c

SRC_S += \
	$(ST_CMSIS)/Source/Templates/gcc/startup_stm32l4r5xx.s

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

# flash target using on-board stlink
flash: flash-stlink
