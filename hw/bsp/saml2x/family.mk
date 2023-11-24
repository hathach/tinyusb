UF2_FAMILY_ID = 0x68ed2b88
DEPS_SUBMODULES += lib/CMSIS_5 hw/mcu/microchip
MCU_DIR = hw/mcu/microchip/$(SAML_VARIANT)

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m0plus

CFLAGS += \
  -nostdlib -nostartfiles \
  -DCONF_OSC32K_CALIB_ENABLE=0 \
  -DCFG_TUSB_MCU=OPT_MCU_SAML22

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=redundant-decls

# SAM driver is flooded with -Wcast-qual which slow down complication significantly
CFLAGS_SKIP += -Wcast-qual

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

SRC_C += \
	src/portable/microchip/samd/dcd_samd.c \
	$(MCU_DIR)/gcc/gcc/startup_$(SAML_VARIANT).c \
	$(MCU_DIR)/gcc/system_$(SAML_VARIANT).c \
	$(MCU_DIR)/hpl/gclk/hpl_gclk.c \
	$(MCU_DIR)/hpl/mclk/hpl_mclk.c \
	$(MCU_DIR)/hpl/pm/hpl_pm.c \
	$(MCU_DIR)/hpl/osc32kctrl/hpl_osc32kctrl.c \
	$(MCU_DIR)/hpl/oscctrl/hpl_oscctrl.c \
	$(MCU_DIR)/hal/src/hal_atomic.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/config \
	$(TOP)/$(MCU_DIR)/include \
	$(TOP)/$(MCU_DIR)/hal/include \
	$(TOP)/$(MCU_DIR)/hal/utils/include \
	$(TOP)/$(MCU_DIR)/hpl/port \
	$(TOP)/$(MCU_DIR)/hri \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include

# flash using bossac at least version 1.8
# can be found in arduino15/packages/arduino/tools/bossac/
# Add it to your PATH or change BOSSAC variable to match your installation
BOSSAC = bossac

flash-bossac: $(BUILD)/$(PROJECT).bin
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(BOSSAC) --port=$(SERIAL) -U -i --offset=0x2000 -e -w $^ -R
