UF2_FAMILY_ID = 0xADA52840
DEPS_SUBMODULES += lib/CMSIS_5 hw/mcu/nordic/nrfx

include $(TOP)/$(BOARD_PATH)/board.mk

# nRF52 is cortex-m4, nRF53 is cortex-m33
CPU_CORE ?= cortex-m4

CFLAGS += \
  -flto \
  -DCFG_TUSB_MCU=OPT_MCU_NRF5X \
  -DCONFIG_GPIO_AS_PINRESET

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=undef -Wno-error=unused-parameter -Wno-error=cast-align -Wno-error=cast-qual -Wno-error=redundant-decls

LDFLAGS += -L$(TOP)/hw/mcu/nordic/nrfx/mdk

SRC_C += \
  src/portable/nordic/nrf5x/dcd_nrf5x.c \
  hw/mcu/nordic/nrfx/drivers/src/nrfx_power.c \
  hw/mcu/nordic/nrfx/drivers/src/nrfx_uarte.c \
  hw/mcu/nordic/nrfx/mdk/system_$(MCU_VARIANT).c

INC += \
  $(TOP)/$(BOARD_PATH) \
  $(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
  $(TOP)/hw/mcu/nordic/nrfx \
  $(TOP)/hw/mcu/nordic/nrfx/mdk \
  $(TOP)/hw/mcu/nordic/nrfx/hal \
  $(TOP)/hw/mcu/nordic/nrfx/drivers/include \
  $(TOP)/hw/mcu/nordic/nrfx/drivers/src \

SRC_S += hw/mcu/nordic/nrfx/mdk/gcc_startup_$(MCU_VARIANT).S

ASFLAGS += -D__HEAP_SIZE=0

# For flash-jlink target
JLINK_DEVICE ?= $(MCU_VARIANT)_xxaa
