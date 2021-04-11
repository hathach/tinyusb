UF2_FAMILY_ID = 0x68ed2b88
DEPS_SUBMODULES += hw/mcu/microchip

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -nostdlib -nostartfiles \
  -DCONF_OSC32K_CALIB_ENABLE=0 \
  -DCFG_TUSB_MCU=OPT_MCU_SAML22

SRC_C += \
	src/portable/microchip/samd/dcd_samd.c \
	hw/mcu/microchip/saml22/gcc/gcc/startup_saml22.c \
	hw/mcu/microchip/saml22/gcc/system_saml22.c \
	hw/mcu/microchip/saml22/hpl/gclk/hpl_gclk.c \
	hw/mcu/microchip/saml22/hpl/mclk/hpl_mclk.c \
	hw/mcu/microchip/saml22/hpl/pm/hpl_pm.c \
	hw/mcu/microchip/saml22/hpl/osc32kctrl/hpl_osc32kctrl.c \
	hw/mcu/microchip/saml22/hpl/oscctrl/hpl_oscctrl.c \
	hw/mcu/microchip/saml22/hal/src/hal_atomic.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/hw/mcu/microchip/saml22/ \
	$(TOP)/hw/mcu/microchip/saml22/config \
	$(TOP)/hw/mcu/microchip/saml22/include \
	$(TOP)/hw/mcu/microchip/saml22/hal/include \
	$(TOP)/hw/mcu/microchip/saml22/hal/utils/include \
	$(TOP)/hw/mcu/microchip/saml22/hpl/port \
	$(TOP)/hw/mcu/microchip/saml22/hri \
	$(TOP)/hw/mcu/microchip/saml22/CMSIS/Core/Include

# For TinyUSB port source 
VENDOR = microchip
CHIP_FAMILY = samd

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# flash using bossac at least version 1.8
# can be found in arduino15/packages/arduino/tools/bossac/
# Add it to your PATH or change BOSSAC variable to match your installation
BOSSAC = bossac

flash-bossac: $(BUILD)/$(PROJECT).bin
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(BOSSAC) --port=$(SERIAL) -U -i --offset=0x2000 -e -w $^ -R
