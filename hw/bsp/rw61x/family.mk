UF2_FAMILY_ID = 0x2abc77ec
SDK_DIR = hw/mcu/nxp/mcux-sdk

include $(TOP)/$(BOARD_PATH)/board.mk

# Default to Highspeed PORT1
PORT ?= 1

CFLAGS += \
  -flto \
  -DBOARD_TUD_RHPORT=$(PORT) \
  -DBOARD_TUH_RHPORT=$(PORT) \
  -DSERIAL_PORT_TYPE_UART=1 \
  -DBOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED \
  -DBOARD_TUH_MAX_SPEED=OPT_MODE_HIGH_SPEED	\

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=old-style-declaration -Wno-error=redundant-decls

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

# All source paths should be relative to the top level.
LD_FILE ?= $(SDK_DIR)/devices/$(MCU_VARIANT)/gcc/$(MCU_CORE)_flash.ld

SRC_C += \
	src/portable/chipidea/ci_hs/dcd_ci_hs.c \
	src/portable/chipidea/ci_hs/hcd_ci_hs.c \
	src/portable/ehci/ehci.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/system_$(MCU_CORE).c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_clock.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_reset.c \
	$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers/fsl_power.c \
	$(SDK_DIR)/drivers/lpc_gpio/fsl_gpio.c \
	$(SDK_DIR)/drivers/common/fsl_common_arm.c\
	$(SDK_DIR)/drivers/flexcomm/fsl_flexcomm.c \
	$(SDK_DIR)/drivers/flexcomm/usart/fsl_usart.c \
	$(SDK_DIR)/drivers/flexspi/fsl_flexspi.c \

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(SDK_DIR)/devices/$(MCU_VARIANT) \
	$(TOP)/$(SDK_DIR)/devices/$(MCU_VARIANT)/drivers \
	$(TOP)/$(SDK_DIR)/drivers/ \
	$(TOP)/$(SDK_DIR)/drivers/common\
	$(TOP)/$(SDK_DIR)/drivers/lpc_gpio\
	$(TOP)/$(SDK_DIR)/drivers/flexcomm \
    $(TOP)/$(SDK_DIR)/drivers/flexcomm/usart \
    $(TOP)/$(SDK_DIR)/drivers/flexspi \


SRC_S += $(SDK_DIR)/devices/$(MCU_VARIANT)/gcc/startup_$(MCU_CORE).S
