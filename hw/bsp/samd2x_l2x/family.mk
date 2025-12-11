UF2_FAMILY_ID = 0x68ed2b88

include $(TOP)/$(BOARD_PATH)/board.mk

# SAM_FAMILY should be set by board.mk (samd21, saml21, or saml22)
ifeq ($(SAM_FAMILY),)
  # Default to samd21 if not specified for backward compatibility
  SAM_FAMILY = samd21
endif

SDK_DIR = hw/mcu/microchip/$(SAM_FAMILY)
CPU_CORE ?= cortex-m0plus

# Common CFLAGS
CFLAGS += \
  -flto \

# Family-specific CFLAGS
ifeq ($(SAM_FAMILY),samd21)
  CFLAGS += \
    -DCONF_DFLL_OVERWRITE_CALIBRATION=0 \
    -DCFG_TUSB_MCU=OPT_MCU_SAMD21
else
  # SAML21/SAML22
  CFLAGS += \
    -DCONF_OSC32K_CALIB_ENABLE=0 \
    -DCFG_EXAMPLE_VIDEO_READONLY \

  ifeq ($(SAM_FAMILY),saml21)
    CFLAGS += -DCFG_TUSB_MCU=OPT_MCU_SAML21
  else ifeq ($(SAM_FAMILY),saml22)
    CFLAGS += -DCFG_TUSB_MCU=OPT_MCU_SAML22
  endif
endif

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=redundant-decls

# SAM driver is flooded with -Wcast-qual which slow down complication significantly
CFLAGS_SKIP += -Wcast-qual

LDFLAGS_GCC += \
  -nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \

LDFLAGS_CLANG +=

# Common source files
SRC_C += \
	src/portable/microchip/samd/dcd_samd.c \
	${SDK_DIR}/gcc/gcc/startup_$(SAM_FAMILY).c \
	${SDK_DIR}/gcc/system_$(SAM_FAMILY).c \
	${SDK_DIR}/hal/src/hal_atomic.c \
	${SDK_DIR}/hpl/gclk/hpl_gclk.c \

# Family-specific source files
ifeq ($(SAM_FAMILY),samd21)
  SRC_C += \
    src/portable/microchip/samd/hcd_samd.c \
    ${SDK_DIR}/hpl/pm/hpl_pm.c \
    ${SDK_DIR}/hpl/sysctrl/hpl_sysctrl.c \

else
  # SAML21/SAML22
  SRC_C += \
    ${SDK_DIR}/hpl/mclk/hpl_mclk.c \
    ${SDK_DIR}/hpl/osc32kctrl/hpl_osc32kctrl.c \
    ${SDK_DIR}/hpl/oscctrl/hpl_oscctrl.c \
    ${SDK_DIR}/hpl/pm/hpl_pm.c \

endif

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/${SDK_DIR} \
	$(TOP)/${SDK_DIR}/config \
	$(TOP)/${SDK_DIR}/include \
	$(TOP)/${SDK_DIR}/hal/include \
	$(TOP)/${SDK_DIR}/hal/utils/include \
	$(TOP)/${SDK_DIR}/hpl/pm/ \
	$(TOP)/${SDK_DIR}/hpl/port \
	$(TOP)/${SDK_DIR}/hri \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \

# flash using bossac at least version 1.8
# can be found in arduino15/packages/arduino/tools/bossac/
# Add it to your PATH or change BOSSAC variable to match your installation
BOSSAC = bossac

flash-bossac: $(BUILD)/$(PROJECT).bin
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(BOSSAC) --port=$(SERIAL) -U -i --offset=0x2000 -e -w $^ -R
