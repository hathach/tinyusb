UF2_FAMILY_ID = 0x55114460
DEPS_SUBMODULES += hw/mcu/microchip

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m4

CFLAGS += \
  -flto \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_SAMD51

# SAM driver is flooded with -Wcast-qual which slow down complication significantly
CFLAGS_SKIP += -Wcast-qual

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

SRC_C += \
	src/portable/microchip/samd/dcd_samd.c \
	hw/mcu/microchip/samd51/gcc/gcc/startup_samd51.c \
	hw/mcu/microchip/samd51/gcc/system_samd51.c \
	hw/mcu/microchip/samd51/hpl/gclk/hpl_gclk.c \
	hw/mcu/microchip/samd51/hpl/mclk/hpl_mclk.c \
	hw/mcu/microchip/samd51/hpl/osc32kctrl/hpl_osc32kctrl.c \
	hw/mcu/microchip/samd51/hpl/oscctrl/hpl_oscctrl.c \
	hw/mcu/microchip/samd51/hal/src/hal_atomic.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/hw/mcu/microchip/samd51/ \
	$(TOP)/hw/mcu/microchip/samd51/config \
	$(TOP)/hw/mcu/microchip/samd51/include \
	$(TOP)/hw/mcu/microchip/samd51/hal/include \
	$(TOP)/hw/mcu/microchip/samd51/hal/utils/include \
	$(TOP)/hw/mcu/microchip/samd51/hpl/port \
	$(TOP)/hw/mcu/microchip/samd51/hri \
	$(TOP)/hw/mcu/microchip/samd51/CMSIS/Include

# flash using bossac at least version 1.8
# can be found in arduino15/packages/arduino/tools/bossac/
# Add it to your PATH or change BOSSAC variable to match your installation
BOSSAC = bossac

flash-bossac: $(BUILD)/$(PROJECT).bin
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(BOSSAC) --port=$(SERIAL) -U -i --offset=0x4000 -e -w $^ -R
