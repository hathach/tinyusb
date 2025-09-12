UF2_FAMILY_ID = 0x70d16657
ST_FAMILY = wba

ST_PREFIX = stm32${ST_FAMILY}xx
ST_CMSIS = hw/mcu/st/cmsis-device-$(ST_FAMILY)
ST_HAL_DRIVER = hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m33

CFLAGS += \
  -DCFG_TUSB_MCU=OPT_MCU_STM32WBA

CFLAGS_GCC += \
  -flto \
  -Wno-error=cast-align -Wno-unused-parameter

LDFLAGS_GCC += \
  -nostdlib -nostartfiles \
  -specs=nosys.specs -specs=nano.specs -Wl,--gc-sections

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	src/portable/synopsys/dwc2/hcd_dwc2.c \
	src/portable/synopsys/dwc2/dwc2_common.c \
	$(ST_CMSIS)/Source/Templates/system_${ST_PREFIX}.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_icache.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_pwr.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_pwr_ex.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_uart.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_pcd.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_hal_pcd_ex.c \
	$(ST_HAL_DRIVER)/Src/${ST_PREFIX}_ll_usb.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(BOARD_PATH)/include \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc

# STM32WBA HAL uses uppercase MCU_VARIANT (excluding the x's) for linking and lowercase MCU_VARIANT for startup.
UPPERCASE_MCU_VARIANT = $(subst XX,xx,$(call to_upper,$(MCU_VARIANT)))

# Startup - Manually specify lowercase version for startup file
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_$(MCU_VARIANT).s
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_$(MCU_VARIANT).s

# Linker
LD_FILE_GCC ?= ${FAMILY_PATH}/linker/${UPPERCASE_MCU_VARIANT}_FLASH_ns.ld
LD_FILE_IAR ?= $(ST_CMSIS)/Source/Templates/iar/linker/$(MCU_VARIANT)_flash_ns.icf

# flash target using on-board stlink
flash: flash-stlink
