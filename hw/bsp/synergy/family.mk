RENESAS_SSP_PATH ?= hw/mcu/renesas/synergy/ssp
include $(TOP)/$(BOARD_PATH)/board.mk

ifeq ($(TOOLCHAIN),clang)
$(error Renesas Synergy SSP does not build with Clang; use TOOLCHAIN=gcc)
endif

ifeq ($(wildcard $(TOP)/$(RENESAS_SSP_PATH)/inc/bsp/bsp_api.h),)
$(error Renesas Synergy SSP not found at '$(TOP)/$(RENESAS_SSP_PATH)'. Install SSP under its license terms and set RENESAS_SSP_PATH to the SSP root containing inc/ and src/)
endif
ifeq ($(wildcard $(TOP)/$(RENESAS_SSP_PATH)/src/bsp/mcu/all/bsp_common.c),)
$(error Renesas Synergy SSP is incomplete at '$(TOP)/$(RENESAS_SSP_PATH)'; expected src/bsp/mcu/all/bsp_common.c)
endif
ifeq ($(wildcard $(TOP)/$(RENESAS_SSP_PATH)/src/bsp/cmsis/Device/RENESAS/$(MCU_VARIANT_UPPERCASE)/Source/startup_$(MCU_VARIANT_UPPERCASE).c),)
$(error Renesas Synergy SSP is incomplete at '$(TOP)/$(RENESAS_SSP_PATH)' for MCU variant $(MCU_VARIANT))
endif

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
  -DCFG_TUSB_MCU=OPT_MCU_SYNERGY \
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
  ${BOARD_PATH}/synergy_gen/pin_data.c

# SSP sources, relative to RENESAS_SSP_PATH. Their objects go under $(BUILD)/obj/ssp/: the generic
# rule would keep the path as given, so a "../" in RENESAS_SSP_PATH would put them outside obj/.
SSP_SRC_C = \
  src/bsp/cmsis/Device/RENESAS/$(MCU_VARIANT_UPPERCASE)/Source/startup_$(MCU_VARIANT_UPPERCASE).c \
  src/bsp/cmsis/Device/RENESAS/$(MCU_VARIANT_UPPERCASE)/Source/system_$(MCU_VARIANT_UPPERCASE).c \
  src/bsp/mcu/all/bsp_common.c \
  src/bsp/mcu/all/bsp_delay.c \
  src/bsp/mcu/all/bsp_irq.c \
  src/bsp/mcu/all/bsp_locking.c \
  src/bsp/mcu/all/bsp_register_protection.c \
  src/bsp/mcu/all/bsp_sbrk.c \
  src/bsp/mcu/$(MCU_VARIANT)/bsp_cache.c \
  src/bsp/mcu/$(MCU_VARIANT)/bsp_clocks.c \
  src/bsp/mcu/$(MCU_VARIANT)/bsp_feature.c \
  src/bsp/mcu/$(MCU_VARIANT)/bsp_group_irq.c \
  src/bsp/mcu/$(MCU_VARIANT)/bsp_module_stop.c \
  src/bsp/mcu/$(MCU_VARIANT)/bsp_rom_registers.c \
  src/driver/r_cgc/r_cgc.c \
  src/driver/r_elc/r_elc.c \
  src/driver/r_ioport/r_ioport.c

SRC_C += $(addprefix ssp/,$(SSP_SRC_C))

# recipe lines must start with a tab
$(BUILD)/obj/ssp/%.o: $(TOP)/$(RENESAS_SSP_PATH)/%.c
	@echo CC $(notdir $@)
	@$(CC) $(CFLAGS) -c -o $@ $<

INC += \
  $(TOP)/$(BOARD_PATH) \
  $(TOP)/$(BOARD_PATH)/synergy_cfg/ssp_cfg/bsp \
  $(TOP)/$(BOARD_PATH)/synergy_cfg/ssp_cfg/driver \
  $(TOP)/$(BOARD_PATH)/synergy_gen \
  $(TOP)/lib/CMSIS_6/CMSIS/Core/Include \
  $(TOP)/$(RENESAS_SSP_PATH)/src/bsp/cmsis/Device/RENESAS/$(MCU_VARIANT_UPPERCASE)/Include \
  $(TOP)/$(RENESAS_SSP_PATH)/inc \
  $(TOP)/$(RENESAS_SSP_PATH)/inc/bsp \
  $(TOP)/$(RENESAS_SSP_PATH)/inc/driver/api \
  $(TOP)/$(RENESAS_SSP_PATH)/inc/driver/instances \
  $(TOP)/$(RENESAS_SSP_PATH)/src/bsp/mcu/all \
  $(TOP)/$(RENESAS_SSP_PATH)/src/bsp/mcu/$(MCU_VARIANT) \

LDFLAGS += -L$(TOP)/$(BOARD_PATH)/script
LDFLAGS += -Wl,--defsym=end=__bss_end__
