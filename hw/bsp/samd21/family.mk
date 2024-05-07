UF2_FAMILY_ID = 0x68ed2b88
SDK_DIR = hw/mcu/microchip/samd21

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m0plus

CFLAGS += \
  -flto \
  -DCONF_DFLL_OVERWRITE_CALIBRATION=0 \
  -DCFG_TUSB_MCU=OPT_MCU_SAMD21

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=redundant-decls

# SAM driver is flooded with -Wcast-qual which slow down complication significantly
CFLAGS_SKIP += -Wcast-qual

LDFLAGS_GCC += \
  -nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \

LDFLAGS_CLANG +=

SRC_C += \
	src/portable/microchip/samd/dcd_samd.c \
	${SDK_DIR}/gcc/gcc/startup_samd21.c \
	${SDK_DIR}/gcc/system_samd21.c \
	${SDK_DIR}/hal/src/hal_atomic.c \
	${SDK_DIR}/hpl/gclk/hpl_gclk.c \
	${SDK_DIR}/hpl/pm/hpl_pm.c \
	${SDK_DIR}/hpl/sysctrl/hpl_sysctrl.c \

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
