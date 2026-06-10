UF2_FAMILY_ID = 0x0

include $(TOP)/$(BOARD_PATH)/board.mk

PY32_SDK_NAME ?= PY32F071_Firmware
PY32_SERIES ?= PY32F071
PY32_TEMPLATE ?= PY32F071xx_Templates
PY32_STARTUP ?= startup_py32f071xx.s
PY32_SYSTEM_SOURCE ?= system_py32f071.c
PY32_HAL_PREFIX ?= py32f071

SDK_DIR = hw/mcu/puya/$(PY32_SDK_NAME)
HAL_DIR = $(SDK_DIR)/Drivers/$(PY32_SERIES)_HAL_Driver
CMSIS_DIR = $(SDK_DIR)/Drivers/CMSIS
CMSIS_DEVICE_DIR = $(CMSIS_DIR)/Device/$(PY32_SERIES)

DEPS_SUBMODULES += $(SDK_DIR)

CFLAGS += \
  -DCFG_TUSB_MCU=OPT_MCU_PY32F0 \
  -DUSE_HAL_DRIVER

# Puya HAL leaves some CMSIS-compatible callback parameters intentionally unused.
CFLAGS += -Wno-error=unused-parameter

MCU_DIR = $(SDK_DIR)

SRC_C += \
  src/portable/mentor/musb/dcd_musb.c \
  hw/bsp/py32f0/family.c \
  $(SDK_DIR)/Templates/$(PY32_TEMPLATE)/Src/$(PY32_SYSTEM_SOURCE) \
  $(HAL_DIR)/Src/$(PY32_HAL_PREFIX)_hal.c \
  $(HAL_DIR)/Src/$(PY32_HAL_PREFIX)_hal_cortex.c \
  $(HAL_DIR)/Src/$(PY32_HAL_PREFIX)_hal_flash.c \
  $(HAL_DIR)/Src/$(PY32_HAL_PREFIX)_hal_gpio.c \
  $(HAL_DIR)/Src/$(PY32_HAL_PREFIX)_hal_pwr.c \
  $(HAL_DIR)/Src/$(PY32_HAL_PREFIX)_hal_rcc.c \
  $(HAL_DIR)/Src/$(PY32_HAL_PREFIX)_hal_rcc_ex.c

SRC_S += $(SDK_DIR)/Templates/$(PY32_TEMPLATE)/EIDE/$(PY32_STARTUP)

INC += \
  $(TOP)/hw/bsp/py32f0 \
  $(TOP)/$(CMSIS_DIR)/Include \
  $(TOP)/$(CMSIS_DEVICE_DIR)/Include \
  $(TOP)/$(HAL_DIR)/Inc \
  $(TOP)/hw/bsp/py32f0/boards/$(BOARD)

SKIP_NANOLIB = 1

LDFLAGS += \
  -nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \
  -Wl,--gc-sections \
  -Wl,--print-memory-usage

CPU_CORE ?= cortex-m0plus
