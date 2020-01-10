CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m3 \
  -mfloat-abi=soft \
  -nostdlib -nostartfiles \
  -DSTM32F103xB \
  -DCFG_TUSB_MCU=OPT_MCU_STM32F1

# mcu driver cause following warnings
#CFLAGS += -Wno-error=unused-parameter

ST_HAL_DRIVER = hw/mcu/st/st_driver/STM32F1xx_HAL_Driver
ST_CMSIS = hw/mcu/st/st_driver/CMSIS/Device/ST/STM32F1xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32F103XB_FLASH.ld

SRC_C += \
  $(ST_CMSIS)/Source/Templates/system_stm32f1xx.c \
  $(ST_HAL_DRIVER)/Src/stm32f1xx_hal.c \
  $(ST_HAL_DRIVER)/Src/stm32f1xx_hal_cortex.c \
  $(ST_HAL_DRIVER)/Src/stm32f1xx_hal_rcc.c \
  $(ST_HAL_DRIVER)/Src/stm32f1xx_hal_rcc_ex.c \
  $(ST_HAL_DRIVER)/Src/stm32f1xx_hal_gpio.c

SRC_S += \
  $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f103xb.s

INC += \
  $(TOP)/hw/mcu/st/st_driver/CMSIS/Include \
  $(TOP)/$(ST_CMSIS)/Include \
  $(TOP)/$(ST_HAL_DRIVER)/Inc \
  $(TOP)/hw/bsp/$(BOARD)

# For TinyUSB port source
VENDOR = st
CHIP_FAMILY = stm32_fsdev

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = stm32f103c8
JLINK_IF = swd

# flash target ROM bootloader
flash: $(BUILD)/$(BOARD)-firmware.bin
	dfu-util -R -a 0 --dfuse-address 0x08000000 -D $<
