SSP = hw/mcu/renesas/synergy/ssp
include $(TOP)/$(BOARD_PATH)/board.mk

# Don't include options setting in .bin file since it create unnecessary large file due to padding
OBJCOPY_BIN_OPTION = --only-section .text --only-section .data --only-section .rodata --only-section .bss

# ----------------------
# Port & Speed Selection
# ----------------------
RHPORT_SPEED ?= OPT_MODE_FULL_SPEED OPT_MODE_HIGH_SPEED
RHPORT_DEVICE ?= 0
RHPORT_HOST ?= 0

# Determine RHPORT_DEVICE_SPEED if not defined
ifndef RHPORT_DEVICE_SPEED
ifeq ($(RHPORT_DEVICE), 0)
  RHPORT_DEVICE_SPEED = $(firstword $(RHPORT_SPEED))
else
  RHPORT_DEVICE_SPEED = $(lastword $(RHPORT_SPEED))
endif
endif

# Determine RHPORT_HOST_SPEED if not defined
ifndef RHPORT_HOST_SPEED
ifeq ($(RHPORT_HOST), 0)
  RHPORT_HOST_SPEED = $(firstword $(RHPORT_SPEED))
else
  RHPORT_HOST_SPEED = $(lastword $(RHPORT_SPEED))
endif
endif

# --------------
# Compiler Flags
# --------------
CFLAGS += \
  -DCFG_TUSB_MCU=OPT_MCU_SYNERGY\
	-DBOARD_TUD_RHPORT=${RHPORT_DEVICE} \
	-DBOARD_TUD_MAX_SPEED=${RHPORT_DEVICE_SPEED} \
	-DBOARD_TUH_RHPORT=${RHPORT_HOST} \
	-DBOARD_TUH_MAX_SPEED=${RHPORT_HOST_SPEED}

CFLAGS += \
  -flto \
	-Wno-error=undef \
	-Wno-error=strict-prototypes \
	-Wno-error=cast-align \
	-Wno-error=cast-qual \
	-Wno-error=unused-but-set-variable \
	-Wno-error=unused-variable \
	-Wno-error=redundant-decls \
	-ffreestanding

LDFLAGS += \
	-nostartfiles -nostdlib \
  -specs=nosys.specs -specs=nano.specs

# -----------------
# Sources & Include
# -----------------
SRC_C += \
	src/portable/renesas/rusb2/dcd_rusb2.c \
	src/portable/renesas/rusb2/hcd_rusb2.c \
	src/portable/renesas/rusb2/rusb2_common.c \
	${BOARD_PATH}/synergy_gen/common_data.c \
	${BOARD_PATH}/synergy_gen/pin_data.c \
	$(SSP)/src/bsp/cmsis/Device/RENESAS/$(MCU_VARIANT_UPPERCASE)/Source/startup_$(MCU_VARIANT_UPPERCASE).c \
	$(SSP)/src/bsp/cmsis/Device/RENESAS/$(MCU_VARIANT_UPPERCASE)/Source/system_$(MCU_VARIANT_UPPERCASE).c \
	$(SSP)/src/bsp/mcu/all/bsp_common.c \
	$(SSP)/src/bsp/mcu/all/bsp_delay.c \
	$(SSP)/src/bsp/mcu/all/bsp_irq.c \
	$(SSP)/src/bsp/mcu/all/bsp_locking.c \
	$(SSP)/src/bsp/mcu/all/bsp_register_protection.c \
	$(SSP)/src/bsp/mcu/all/bsp_sbrk.c \
	$(SSP)/src/bsp/mcu/$(MCU_VARIANT)/bsp_cache.c \
	$(SSP)/src/bsp/mcu/$(MCU_VARIANT)/bsp_clocks.c \
	$(SSP)/src/bsp/mcu/$(MCU_VARIANT)/bsp_feature.c \
	$(SSP)/src/bsp/mcu/$(MCU_VARIANT)/bsp_group_irq.c \
	$(SSP)/src/bsp/mcu/$(MCU_VARIANT)/bsp_module_stop.c \
	$(SSP)/src/bsp/mcu/$(MCU_VARIANT)/bsp_rom_registers.c \
	$(SSP)/src/driver/r_cgc/r_cgc.c \
	$(SSP)/src/driver/r_elc/r_elc.c \
	$(SSP)/src/driver/r_ioport/r_ioport.c \

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(BOARD_PATH)/synergy_cfg/ssp_cfg/bsp \
	$(TOP)/$(BOARD_PATH)/synergy_cfg/ssp_cfg/driver \
	$(TOP)/$(BOARD_PATH)/synergy_gen \
	$(TOP)/lib/CMSIS_6/CMSIS/Core/Include \
	$(TOP)/$(SSP)/src/bsp/cmsis/Device/RENESAS/$(MCU_VARIANT_UPPERCASE)/Include \
	$(TOP)/$(SSP)/inc \
	$(TOP)/$(SSP)/inc/bsp \
	$(TOP)/$(SSP)/inc/driver/api \
	$(TOP)/$(SSP)/inc/driver/instances \
	$(TOP)/$(SSP)/src/bsp/mcu/all \
	$(TOP)/$(SSP)/src/bsp/mcu/$(MCU_VARIANT) \

LIBS += \
	$(TOP)/$(SSP)/src/driver/r_fmi/libs/libfmi_cm0_$(MCU_VARIANT)_gcc.a \
	$(TOP)/$(SSP)/src/bsp/mcu/$(MCU_VARIANT)/libfmi_R7FS124773A01CFM_gcc.a

ifndef LD_FILE
LD_FILE = $(BOARD_PATH)/script/r7fs124773a01cfm.ld
endif

LDFLAGS += -L$(TOP)/$(BOARD_PATH)/script
LDFLAGS += -Wl,--defsym=end=__bss_end__
