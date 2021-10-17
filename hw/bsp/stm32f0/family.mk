UF2_FAMILY_ID = 0x647824b6
ST_FAMILY = f0
DEPS_SUBMODULES += lib/CMSIS_5 hw/mcu/st/cmsis_device_$(ST_FAMILY) hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

ST_CMSIS = hw/mcu/st/cmsis_device_$(ST_FAMILY)
ST_HAL_DRIVER = hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0 \
  -mfloat-abi=soft \
  -nostdlib -nostartfiles \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_TUSB_MCU=OPT_MCU_STM32F0

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=unused-parameter -Wno-error=cast-align -Wno-error=cast-qual

SRC_C += \
  src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
  $(ST_CMSIS)/Source/Templates/system_stm32$(ST_FAMILY)xx.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_cortex.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc_ex.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_gpio.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart.c

INC += \
	$(TOP)/$(BOARD_PATH) \
  $(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
  $(TOP)/$(ST_CMSIS)/Include \
  $(TOP)/$(ST_HAL_DRIVER)/Inc

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0
