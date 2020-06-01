CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m7 \
  -mfloat-abi=hard \
  -mfpu=fpv5-d16 \
  -nostdlib -nostartfiles \
  -DSTM32H743xx \
  -DCFG_TUSB_MCU=OPT_MCU_STM32H7 \
	-DBOARD_DEVICE_RHPORT_NUM=1 \
	-DBOARD_DEVICE_RHPORT_SPEED=OPT_MODE_HIGH_SPEED

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=maybe-uninitialized -Wno-error=cast-align

ST_HAL_DRIVER = hw/mcu/st/st_driver/STM32H7xx_HAL_Driver
ST_CMSIS = hw/mcu/st/st_driver/CMSIS/Device/ST/STM32H7xx

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/STM32H743XIHx_FLASH.ld

SRC_C += \
	$(ST_CMSIS)/Source/Templates/system_stm32h7xx.c \
	$(ST_HAL_DRIVER)/Src/stm32h7xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32h7xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32h7xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32h7xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32h7xx_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/stm32h7xx_hal_uart.c \
	$(ST_HAL_DRIVER)/Src/stm32h7xx_hal_pwr_ex.c

SRC_S += \
	$(ST_CMSIS)/Source/Templates/gcc/startup_stm32h743xx.s

INC += \
	$(TOP)/hw/mcu/st/st_driver/CMSIS/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc \
	$(TOP)/hw/bsp/$(BOARD)

# For TinyUSB port source
VENDOR = st
CHIP_FAMILY = synopsys

# For freeRTOS port source
FREERTOS_PORT = ARM_CM7/r0p1

# For flash-jlink target
JLINK_DEVICE = stm32h743xi
JLINK_IF = swd

# flash target using on-board stlink
flash: flash-stlink
