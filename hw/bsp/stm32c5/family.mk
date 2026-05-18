ST_FAMILY = c5
ST_CMSIS = hw/mcu/st/stm32$(ST_FAMILY)xx-dfp
ST_DRIVER = hw/mcu/st/stm32$(ST_FAMILY)xx-drivers

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m33

# --------------
# Compiler Flags
# --------------
CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_STM32C5 \

# GCC Flags
CFLAGS += \
  -flto \

# suppress warnings caused by vendor mcu driver
CFLAGS += -Wno-error=cast-align -Wno-error=unused-parameter -Wno-error=redundant-decls

LDFLAGS += \
  -nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs

# -----------------
# Sources & Include
# -----------------

SRC_C += \
	src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
	src/portable/st/stm32_fsdev/hcd_stm32_fsdev.c \
	src/portable/st/stm32_fsdev/fsdev_common.c \
	$(ST_CMSIS)/Source/Templates/system_stm32$(ST_FAMILY)xx.c \
	$(ST_CMSIS)/Source/startup_$(MCU_VARIANT).c \
	$(ST_DRIVER)/hal/stm32$(ST_FAMILY)xx_hal.c \
	$(ST_DRIVER)/hal/stm32$(ST_FAMILY)xx_hal_cortex.c \
	$(ST_DRIVER)/hal/stm32$(ST_FAMILY)xx_hal_flash_itf.c \
	$(ST_DRIVER)/hal/stm32$(ST_FAMILY)xx_hal_pwr.c \
	$(ST_DRIVER)/hal/stm32$(ST_FAMILY)xx_hal_rcc.c \
	$(ST_DRIVER)/hal/stm32$(ST_FAMILY)xx_hal_gpio.c \
	$(ST_DRIVER)/hal/stm32$(ST_FAMILY)xx_hal_uart.c \
	$(ST_DRIVER)/hal/stm32$(ST_FAMILY)xx_hal_dma.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_6/CMSIS/Core/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_DRIVER)/hal \
	$(TOP)/$(ST_DRIVER)/ll

# flash target using on-board stlink
flash: flash-stlink
