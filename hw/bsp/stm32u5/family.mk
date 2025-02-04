ST_FAMILY = u5
DEPS_SUBMODULES += lib/CMSIS_5 hw/mcu/st/cmsis_device_$(ST_FAMILY) hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

ST_CMSIS = hw/mcu/st/cmsis_device_$(ST_FAMILY)
ST_HAL_DRIVER = hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m33

CFLAGS += \
  -DCFG_TUSB_MCU=OPT_MCU_STM32U5

# suppress warning caused by vendor mcu driver
CFLAGS_GCC += \
  -flto \
  -Wno-error=cast-align \
  -Wno-error=undef \
  -Wno-error=unused-parameter \
  -Wno-error=type-limits \

ifeq ($(TOOLCHAIN),gcc)
CFLAGS_GCC += -Wno-error=maybe-uninitialized
endif

LDFLAGS_GCC += \
  -nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs

SRC_C += \
	$(ST_CMSIS)/Source/Templates/system_stm32$(ST_FAMILY)xx.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_icache.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_pwr.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_pwr_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart.c

ifeq ($(MCU_VARIANT),stm32u545xx)
SRC_C += \
	src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
else ifeq ($(MCU_VARIANT),stm32u535xx)
SRC_C += \
	src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
else
SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	src/portable/synopsys/dwc2/hcd_dwc2.c \
	src/portable/synopsys/dwc2/dwc2_common.c
endif

INC += \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc \
	$(TOP)/$(BOARD_PATH)

# flash target using on-board stlink
flash: flash-stlink
