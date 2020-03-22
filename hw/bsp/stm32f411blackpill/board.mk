CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -DSTM32F411xE \
  -DCFG_TUSB_MCU=OPT_MCU_STM32F4

ST_HAL_DRIVER = hw/mcu/st/st_driver/STM32F4xx_HAL_Driver
ST_CMSIS = hw/mcu/st/st_driver/CMSIS/Device/ST/STM32F4xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32F411CEUx_FLASH.ld

SRC_C += \
	$(ST_CMSIS)/Source/Templates/system_stm32f4xx.c \
	$(ST_HAL_DRIVER)/Src/stm32f4xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32f4xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32f4xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32f4xx_hal_gpio.c

SRC_S += \
	$(ST_CMSIS)/Source/Templates/gcc/startup_stm32f411xe.s

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
JLINK_DEVICE = stm32f411ce
JLINK_IF = swd

# flash target ROM bootloader
flash: $(BUILD)/$(BOARD)-firmware.bin
	dfu-util -R -a 0 --dfuse-address 0x08000000 -D $<
