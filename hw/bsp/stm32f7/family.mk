UF2_FAMILY_ID = 0x53b80f00
ST_FAMILY = f7
DEPS_SUBMODULES += lib/CMSIS_5 hw/mcu/st/cmsis_device_$(ST_FAMILY) hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

ST_CMSIS = hw/mcu/st/cmsis_device_$(ST_FAMILY)
ST_HAL_DRIVER = hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

include $(TOP)/$(BOARD_PATH)/board.mk

# --------------
# Compiler Flags
# --------------
CFLAGS += \
  -DCFG_TUSB_MCU=OPT_MCU_STM32F7 \
  -DBOARD_TUD_RHPORT=$(PORT)

ifeq ($(PORT), 1)
  ifeq ($(SPEED), high)
    CFLAGS += -DBOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
    $(info "Using OTG_HS in HighSpeed mode")
  else
    CFLAGS += -DBOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
    $(info "Using OTG_HS in FullSpeed mode")
  endif
else
  $(info "Using OTG_FS")
endif

# GCC Flags
GCC_CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m7 \
  -mfloat-abi=hard \
  -mfpu=fpv5-d16 \
  -nostdlib -nostartfiles

# mcu driver cause following warnings
GCC_CFLAGS += -Wno-error=shadow -Wno-error=cast-align

# IAR Flags
IAR_CFLAGS += --cpu cortex-m7 --fpu VFPv5_D16
IAR_ASFLAGS += --cpu cortex-m7 --fpu VFPv5_D16

# -----------------
# Sources & Include
# -----------------

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	$(ST_CMSIS)/Source/Templates/system_stm32$(ST_FAMILY)xx.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_pwr_ex.c

INC += \
  $(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc

# For freeRTOS port source
FREERTOS_PORT = ARM_CM7/r0p1
