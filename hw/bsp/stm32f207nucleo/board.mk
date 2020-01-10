CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -mfloat-abi=soft \
  -nostdlib -nostartfiles \
  -DSTM32F207xx \
  -DCFG_TUSB_MCU=OPT_MCU_STM32F2

# mcu driver cause following warnings
CFLAGS += -Wno-error=sign-compare

ST_HAL_DRIVER = hw/mcu/st/st_driver/STM32F2xx_HAL_Driver
ST_CMSIS = hw/mcu/st/st_driver/CMSIS/Device/ST/STM32F2xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32F207ZGTx_FLASH.ld

SRC_C += \
  $(ST_CMSIS)/Source/Templates/system_stm32f2xx.c \
  $(ST_HAL_DRIVER)/Src/stm32f2xx_hal.c \
  $(ST_HAL_DRIVER)/Src/stm32f2xx_hal_cortex.c \
  $(ST_HAL_DRIVER)/Src/stm32f2xx_hal_rcc.c \
  $(ST_HAL_DRIVER)/Src/stm32f2xx_hal_rcc_ex.c \
  $(ST_HAL_DRIVER)/Src/stm32f2xx_hal_gpio.c

SRC_S += \
  $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f207xx.s

INC += \
  $(TOP)/hw/mcu/st/st_driver/CMSIS/Include \
  $(TOP)/$(ST_CMSIS)/Include \
  $(TOP)/$(ST_HAL_DRIVER)/Inc \
  $(TOP)/hw/bsp/$(BOARD)

# For TinyUSB port source
VENDOR = st
CHIP_FAMILY = synopsys

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = stm32f207zg
JLINK_IF = swd

# flash target using on-board stlink
flash: flash-stlink
