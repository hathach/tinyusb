UF2_FAMILY_ID = 0x68ed2b88
DEPS_SUBMODULES += hw/mcu/microchip

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m0plus

CFLAGS += \
  -flto \
  -nostdlib -nostartfiles \
  -DCONF_DFLL_OVERWRITE_CALIBRATION=0 \
  -DCFG_TUSB_MCU=OPT_MCU_SAMD21

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=redundant-decls

# SAM driver is flooded with -Wcast-qual which slow down complication significantly
CFLAGS_SKIP += -Wcast-qual

SRC_C += \
	src/portable/microchip/samd/dcd_samd.c \
	hw/mcu/microchip/samd21/gcc/gcc/startup_samd21.c \
	hw/mcu/microchip/samd21/gcc/system_samd21.c \
	hw/mcu/microchip/samd21/hpl/gclk/hpl_gclk.c \
	hw/mcu/microchip/samd21/hpl/pm/hpl_pm.c \
	hw/mcu/microchip/samd21/hpl/sysctrl/hpl_sysctrl.c \
	hw/mcu/microchip/samd21/hal/src/hal_atomic.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/hw/mcu/microchip/samd21/ \
	$(TOP)/hw/mcu/microchip/samd21/config \
	$(TOP)/hw/mcu/microchip/samd21/include \
	$(TOP)/hw/mcu/microchip/samd21/hal/include \
	$(TOP)/hw/mcu/microchip/samd21/hal/utils/include \
	$(TOP)/hw/mcu/microchip/samd21/hpl/pm/ \
	$(TOP)/hw/mcu/microchip/samd21/hpl/port \
	$(TOP)/hw/mcu/microchip/samd21/hri \
	$(TOP)/hw/mcu/microchip/samd21/CMSIS/Include

# flash using bossac at least version 1.8
# can be found in arduino15/packages/arduino/tools/bossac/
# Add it to your PATH or change BOSSAC variable to match your installation
BOSSAC = bossac

flash-bossac: $(BUILD)/$(PROJECT).bin
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(BOSSAC) --port=$(SERIAL) -U -i --offset=0x2000 -e -w $^ -R
