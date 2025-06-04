ST_FAMILY = n6
ST_PREFIX = stm32${ST_FAMILY}xx
ST_PREFIX_LONG = stm32${ST_FAMILY}57xx
ST_CMSIS = hw/mcu/st/cmsis_device_$(ST_FAMILY)
ST_HAL_DRIVER = hw/mcu/st/${ST_PREFIX}_hal_driver

UF2_FAMILY_ID = 0x6db66083

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m55

# ----------------------
# Port & Speed Selection
# ----------------------
RHPORT_SPEED ?= OPT_MODE_FULL_SPEED OPT_MODE_HIGH_SPEED
RHPORT_DEVICE ?= 1
RHPORT_HOST ?= 1

# Determine RHPORT_DEVICE_SPEED if not defined
ifndef RHPORT_DEVICE_SPEED
ifeq ($(RHPORT_DEVICE), 0)
  RHPORT_DEVICE_SPEED = $(firstword $(RHPORT_SPEED))
else
  RHPORT_DEVICE_SPEED = $(lastword $(RHPORT_SPEED))
endif
endif

# Determine RHPORT_HOST_SPEED if not defined
ifndef RHPORT_HOST_SPEED
ifeq ($(RHPORT_HOST), 0)
  RHPORT_HOST_SPEED = $(firstword $(RHPORT_SPEED))
else
  RHPORT_HOST_SPEED = $(lastword $(RHPORT_SPEED))
endif
endif

# --------------
# Compiler Flags
# --------------
CFLAGS += \
  -DCFG_TUSB_MCU=OPT_MCU_STM32N6 \
	-DBOARD_TUD_RHPORT=${RHPORT_DEVICE} \
	-DBOARD_TUD_MAX_SPEED=${RHPORT_DEVICE_SPEED} \
	-DBOARD_TUH_RHPORT=${RHPORT_HOST} \
	-DBOARD_TUH_MAX_SPEED=${RHPORT_HOST_SPEED}

# GCC Flags
CFLAGS_GCC += \
  -flto \

# suppress warning caused by vendor mcu driver
CFLAGS_GCC += \
  -Wno-error=cast-align \
  -Wno-error=unused-parameter \

LDFLAGS_GCC += \
  -nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs

# -----------------
# Sources & Include
# -----------------

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	src/portable/synopsys/dwc2/hcd_dwc2.c \
	src/portable/synopsys/dwc2/dwc2_common.c \
	$(ST_CMSIS)/Source/Templates/system_${ST_PREFIX}_fsbl.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_dma.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_hcd.c \
    $(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_i2c.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_pcd.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_pcd_ex.c \
    $(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_pwr.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_pwr_ex.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_rif.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_uart.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_uart_ex.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_ll_usb.c \

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc

# Linker
LD_FILE_GCC = $(BOARD_PATH)/STM32N657XX_AXISRAM2_fsbl.ld

# Startup
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_$(ST_PREFIX_LONG)_fsbl.s
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_$(MCU_VARIANT).s

# Linker
LD_FILE_GCC ?= $(ST_CMSIS)/Source/Templates/gcc/linker/$(ST_PREFIX_LONG)_flash.ld
LD_FILE_IAR ?= $(ST_CMSIS)/Source/Templates/iar/linker/$(MCU_VARIANT)_flash.icf
