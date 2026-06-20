ST_FAMILY = u0
ST_CMSIS = hw/mcu/st/cmsis-device-$(ST_FAMILY)
ST_HAL_DRIVER = hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m0plus

CFLAGS += \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_EXAMPLE_VIDEO_READONLY \
  -DCFG_TUSB_MCU=OPT_MCU_STM32U0

# mcu driver cause following warnings
CFLAGS += \
  -flto \
	-Wno-error=unused-parameter \
	-Wno-error=redundant-decls \
	-Wno-error=cast-align \
	-Wno-error=maybe-uninitialized \

CFLAGS_CLANG += \
  -Wno-error=parentheses-equality

LDFLAGS += \
  -nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs

SRC_C += \
  src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
  src/portable/st/stm32_fsdev/fsdev_common.c \
  $(ST_CMSIS)/Source/Templates/system_stm32$(ST_FAMILY)xx.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_cortex.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc_ex.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_gpio.c \
  ${ST_HAL_DRIVER}/Src/stm32$(ST_FAMILY)xx_hal_pwr_ex.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart_ex.c

INC += \
	$(TOP)/$(BOARD_PATH) \
  $(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
  $(TOP)/$(ST_CMSIS)/Include \
  $(TOP)/$(ST_HAL_DRIVER)/Inc

# Startup
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_${MCU_VARIANT}.s

# Linker
MCU_VARIANT_UPPER = $(subst stm32u,STM32U,$(MCU_VARIANT))
LD_FILE ?= $(FAMILY_PATH)/linker/$(MCU_VARIANT_UPPER)_FLASH.ld
