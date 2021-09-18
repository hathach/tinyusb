DEPS_SUBMODULES += hw/mcu/microchip

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -nostdlib -nostartfiles \
  -DCONF_DFLL_OVERWRITE_CALIBRATION=0 \
  -DOSC32K_OVERWRITE_CALIBRATION=0 \
  -DCFG_TUSB_MCU=OPT_MCU_SAMD11

SRC_C += \
	src/portable/microchip/samd/dcd_samd.c \
	hw/mcu/microchip/samd11/gcc/gcc/startup_samd11.c \
	hw/mcu/microchip/samd11/gcc/system_samd11.c \
	hw/mcu/microchip/samd11/hpl/gclk/hpl_gclk.c \
	hw/mcu/microchip/samd11/hpl/pm/hpl_pm.c \
	hw/mcu/microchip/samd11/hpl/sysctrl/hpl_sysctrl.c \
	hw/mcu/microchip/samd11/hal/src/hal_atomic.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/hw/mcu/microchip/samd11/ \
	$(TOP)/hw/mcu/microchip/samd11/config \
	$(TOP)/hw/mcu/microchip/samd11/include \
	$(TOP)/hw/mcu/microchip/samd11/hal/include \
	$(TOP)/hw/mcu/microchip/samd11/hal/utils/include \
	$(TOP)/hw/mcu/microchip/samd11/hpl/pm/ \
	$(TOP)/hw/mcu/microchip/samd11/hpl/port \
	$(TOP)/hw/mcu/microchip/samd11/hri \
	$(TOP)/hw/mcu/microchip/samd11/CMSIS/Include \
	$(TOP)/hw/mcu/microchip/samd11/CMSIS/Core/Include

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0
