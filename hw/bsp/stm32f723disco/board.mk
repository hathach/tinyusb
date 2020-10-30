PORT ?= 1
SPEED ?= high

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m7 \
  -mfloat-abi=hard \
  -mfpu=fpv5-d16 \
  -nostdlib -nostartfiles \
  -DSTM32F723xx \
  -DHSE_VALUE=25000000 \
  -DCFG_TUSB_MCU=OPT_MCU_STM32F7 \
  -DBOARD_DEVICE_RHPORT_NUM=$(PORT)

ifeq ($(PORT), 1)
  ifeq ($(SPEED), high)
    CFLAGS += -DBOARD_DEVICE_RHPORT_SPEED=OPT_MODE_HIGH_SPEED
    $(info "Using OTG_HS in HighSpeed mode")
  else
    CFLAGS += -DBOARD_DEVICE_RHPORT_SPEED=OPT_MODE_FULL_SPEED
    $(info "Using OTG_HS in FullSpeed mode")
  endif
else
  $(info "Using OTG_FS")
endif

# mcu driver cause following warnings
CFLAGS += -Wno-error=shadow -Wno-error=cast-align

ST_FAMILY = f7
ST_CMSIS = hw/mcu/st/cmsis_device_$(ST_FAMILY)
ST_HAL_DRIVER = hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32F723xE_FLASH.ld

SRC_C += \
	$(ST_CMSIS)/Source/Templates/system_stm32$(ST_FAMILY)xx.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_pwr_ex.c

SRC_S += \
	$(ST_CMSIS)/Source/Templates/gcc/startup_stm32f723xx.s

INC += \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc \
	$(TOP)/hw/bsp/$(BOARD)

# For TinyUSB port source
VENDOR = st
CHIP_FAMILY = synopsys

# For freeRTOS port source
FREERTOS_PORT = ARM_CM7/r0p1

# flash target using on-board stlink
flash: flash-stlink
