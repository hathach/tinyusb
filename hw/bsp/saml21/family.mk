UF2_FAMILY_ID = 0x68ed2b88
DEPS_SUBMODULES += hw/mcu/microchip

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -nostdlib -nostartfiles \
  -DCONF_OSC32K_CALIB_ENABLE=0 \
  -DCFG_TUSB_MCU=OPT_MCU_SAML21

SRC_C += \
	src/portable/microchip/samd/dcd_samd.c \
	hw/mcu/microchip/saml21/gcc/gcc/startup_saml21.c \
	hw/mcu/microchip/saml21/gcc/system_saml21.c \
	hw/mcu/microchip/saml21/hpl/gclk/hpl_gclk.c \
	hw/mcu/microchip/saml21/hpl/mclk/hpl_mclk.c \
	hw/mcu/microchip/saml21/hpl/pm/hpl_pm.c \
	hw/mcu/microchip/saml21/hpl/osc32kctrl/hpl_osc32kctrl.c \
	hw/mcu/microchip/saml21/hpl/oscctrl/hpl_oscctrl.c \
	hw/mcu/microchip/saml21/hal/src/hal_atomic.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/hw/mcu/microchip/saml21/ \
	$(TOP)/hw/mcu/microchip/saml21/config \
	$(TOP)/hw/mcu/microchip/saml21/include \
	$(TOP)/hw/mcu/microchip/saml21/hal/include \
	$(TOP)/hw/mcu/microchip/saml21/hal/utils/include \
	$(TOP)/hw/mcu/microchip/saml21/hpl/port \
	$(TOP)/hw/mcu/microchip/saml21/hri \
	$(TOP)/hw/mcu/microchip/saml21/CMSIS/Include

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
